#include "src/game/presentation/TradeDealPresentation.h"

#include <algorithm>
#include <cmath>

#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/EconomyModel.h"

namespace game::presentation
{
using game::economy::CargoCapacity;
using game::economy::CargoLot;
using game::economy::CargoManifest;
using game::economy::CargoRegistry;

game::economy::TradeDealRules dealRulesOf(const game::simulation::ClientTradeSnapshot& s)
{
    game::economy::TradeDealRules r;
    r.kappa = s.kappa;
    r.stockExponent = s.stockExponent;
    r.minStockShare = s.minStockShare;
    r.hubBuyCapShare = s.hubBuyCapShare;
    r.handlingCrPerT = s.handlingCrPerT;   // already x the port index (the style below carries index 1)
    return r;
}

game::economy::FiscalStyle fiscalStyleOf(const game::simulation::ClientTradeSnapshot& s)
{
    game::economy::FiscalStyle st;
    st.ru = s.fiscalStyleRu;
    st.vatShare = s.vatShare;
    st.vatRefundDays = s.vatRefundDays;
    st.turnoverShare = s.turnoverShare;
    st.turnoverRu = s.turnoverRu;
    st.portIndex = 1.0;
    return st;
}

game::economy::MarketSide marketSideOf(const game::simulation::ClientTradeSnapshot& s, const game::simulation::ClientMarketRow& row)
{
    return {row.priceCr, s.spread, row.stockT, row.targetT};
}

std::vector<long long> roundLinesToTotal(const std::vector<double>& cr, long long& totalOut)
{
    double sum = 0.0;
    for (const double c : cr)
        sum += c;
    totalOut = std::llround(sum);
    std::vector<long long> out(cr.size());
    long long shown = 0;
    for (std::size_t i = 0; i < cr.size(); ++i)
    {
        out[i] = static_cast<long long>(std::floor(cr[i]));
        shown += out[i];
    }
    // hand the missing whole Cr to the lines with the largest fractional parts
    std::vector<std::size_t> order(cr.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return cr[a] - std::floor(cr[a]) > cr[b] - std::floor(cr[b]); });
    for (std::size_t k = 0; shown < totalOut && !order.empty(); k = (k + 1) % order.size())
    {
        ++out[order[k]];
        ++shown;
    }
    return out;
}

namespace
{
// the largest q in [0, hi] with ok(q) (ok monotone: true up to some q)
template <class F> double largestOk(double hi, F ok)
{
    if (!(hi > 0.0))
        return 0.0;
    if (ok(hi))
        return hi;
    double lo = 0.0;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        (ok(mid) ? lo : hi) = mid;
    }
    return lo;
}
} // namespace

TradeDealView buildTradeDeal(const game::simulation::ClientTradeSnapshot& snap, const TradeDealRequest& req)
{
    TradeDealView v;
    const auto& goods = game::economy::EconomyModel::instance().goods();
    if (!snap.valid || req.good < 0 || req.good >= static_cast<int>(goods.size()))
        return v;
    const game::simulation::ClientMarketRow* row = nullptr;
    for (const auto& r : snap.market)
        if (r.good == req.good && r.traded)
            row = &r;
    if (!row)
        return v;
    const auto& good = goods[static_cast<std::size_t>(req.good)];
    v.valid = true;
    v.good = req.good;
    v.goodId = good.id;
    v.buy = req.buy;
    v.unitsC20 = req.unitsC20;
    v.docked = snap.docked;
    v.boxT = game::economy::c20NetTonnes(good.densityTM3);
    v.spendableCr = snap.spendableCr;
    v.vatRefundDays = snap.vatRefundDays;
    v.sentUnitCr = req.sentUnitCr;
    v.sentRequest = req.sentRequest;

    const auto rules = dealRulesOf(snap);
    const auto style = fiscalStyleOf(snap);
    const auto side = marketSideOf(snap, *row);

    // the manifest as the server keeps it (the same CargoRegistry rules decide the C20 boxes and the payload)
    CargoManifest m;
    std::string ownOwner;
    for (const auto& c : snap.cargo)
    {
        CargoLot lot;
        lot.lotId = c.lotId;
        lot.goodId = c.good >= 0 && c.good < static_cast<int>(goods.size()) ? goods[static_cast<std::size_t>(c.good)].id : std::string();
        lot.tonnes = c.tonnes;
        lot.c20 = c.c20;
        lot.owner = c.owner;
        lot.contractId = c.contractId;
        lot.originHub = c.originHub;
        lot.loadedDay = c.loadedDay;
        if (c.contractId.empty() && c.owner.rfind("player:", 0) == 0)
            ownOwner = c.owner;
        m.lots.push_back(std::move(lot));
    }
    v.aboardT = m.ownTonnesOf(good.id);
    v.c20Free = std::max(0, snap.c20Slots - m.c20());

    if (req.buy)
    {
        CargoLot lot;   // the lot this purchase would load (it may merge into today's lot from this hub)
        lot.goodId = good.id;
        lot.owner = ownOwner;
        lot.originHub = snap.hubId;
        lot.loadedDay = snap.day;
        const CargoCapacity cap {snap.netCapacityT, snap.c20Slots};
        const auto fitsC20 = [&](double q) {
            lot.tonnes = q;
            return m.c20() + CargoRegistry::extraC20(m, lot, good.densityTM3) <= cap.c20Slots;
        };
        const double payloadT = std::max(0.0, snap.netCapacityT - m.netT());
        v.hubLimitT = row->hubGivesUpToT;
        const double hi = std::max({v.hubLimitT, payloadT, 0.0}) + v.boxT * std::max(1, snap.c20Slots);
        const double c20T = largestOk(hi, fitsC20);
        v.holdLimitT = std::min(c20T, payloadT);
        const double cash = std::max(0.0, snap.spendableCr - 0.01);   // pay() covers to the cent
        v.moneyLimitT = largestOk(std::max(v.hubLimitT, 0.0),
                                  [&](double q) { return -game::economy::explainDeal(true, side, q, style, rules).totalCr <= cash; });
        v.maxT = std::min({v.holdLimitT, v.hubLimitT, v.moneyLimitT});
        v.limitKind = v.maxT == v.moneyLimitT ? "money" : v.maxT == v.hubLimitT ? "hub" : c20T <= payloadT ? "hold_c20" : "payload";
    }
    else
    {
        v.hubLimitT = row->hubTakesUpToT;
        v.maxT = std::min(v.aboardT, v.hubLimitT);
        v.limitKind = v.aboardT <= v.hubLimitT ? "aboard" : "hub";
    }
    v.maxT = std::max(0.0, v.maxT);
    v.tonnes = req.tonnes < 0.0 ? std::min(v.boxT, v.maxT) : std::clamp(req.tonnes, 0.0, v.maxT);
    v.c20 = game::economy::c20For(v.tonnes, good.densityTM3);

    // a sale: the import duty of the lots the server's unload() takes (oldest first), each at the rate the server
    // replicated for this hub (TRADE step 4) - the same CargoRegistry::fifoDutyShare the server charges with
    if (!req.buy)
        v.dutyShare = CargoRegistry::fifoDutyShare(m, good.id, v.tonnes, [&](const CargoLot& lot) {
            for (const auto& c : snap.cargo)
                if (c.lotId == lot.lotId)
                    return c.dutyShare;
            return 0.0;
        });
    v.quote = game::economy::explainDeal(req.buy, side, v.tonnes, style, rules, v.dutyShare);
    if (!(v.tonnes > 0.0))   // nothing to deal: still show the list price (explainDeal leaves it 0 for a zero quantity)
        v.quote.listUnitCr = v.quote.unitCr = req.buy ? game::economy::askCr(side) : game::economy::bidCr(side);
    std::vector<double> amounts;
    const char* kinds[] = {"goods", "handling", "vat", "duty", "turnover"};
    const double values[] = {req.buy ? -v.quote.goodsCr : v.quote.goodsCr, -v.quote.handlingCr, -v.quote.vatCr, -v.quote.dutyCr,
                             -v.quote.turnoverCr};
    // the same lines, in the same order, as explainDeal books (a zero VAT / duty / turnover line is not booked)
    for (int i = 0; i < 5; ++i)
        if (i < 2 || (i == 2 && v.quote.vatCr > 0.0) || (i == 3 && !req.buy && v.quote.dutyCr > 0.0) ||
            (i == 4 && !req.buy && v.quote.turnoverCr > 0.0))
        {
            v.lines.push_back({kinds[i], values[i], 0});
            amounts.push_back(values[i]);
        }
    const auto shown = roundLinesToTotal(amounts, v.shownTotalCr);
    for (std::size_t i = 0; i < v.lines.size(); ++i)
        v.lines[i].shownCr = shown[i];
    if (req.buy && style.vatRefundDays > 0)
        v.vatRefundCr = v.quote.vatReclaimCr * (1.0 - rules.vatRefundFeeShare);

    // the server's answer to the last Confirm
    if (req.sentRequest != 0)
    {
        v.answer = TradeDealAnswer::Waiting;
        for (const auto& r : snap.results)
            if (r.requestId == req.sentRequest)
            {
                v.result = r;
                v.answer = r.ok ? TradeDealAnswer::Ok
                         : (r.code == "price_moved" || r.code == "requote") ? TradeDealAnswer::PriceMoved
                                                                             : TradeDealAnswer::Refused;
            }
    }

    if (!snap.docked)
        v.blockKind = "not_docked";
    else if (v.answer == TradeDealAnswer::Waiting)
        v.blockKind = "waiting";
    else if (!(v.tonnes > 0.0))
        v.blockKind = "zero";
    else if (!v.quote.ok)
        v.blockKind = "quote";
    else if (v.quote.totalCr < 0.0 && -v.quote.totalCr > snap.spendableCr)
        v.blockKind = "money";
    v.canConfirm = v.blockKind.empty();
    return v;
}

double tradeDealStep(const TradeDealView& v, int steps)
{
    if (!v.valid)
        return 0.0;
    double q;
    if (v.unitsC20)
    {
        // whole boxes: from a part-filled box the first step lands on the box boundary
        const double boxes = v.tonnes / v.boxT;
        const double base = steps > 0 ? std::floor(boxes + 1.0e-6) : std::ceil(boxes - 1.0e-6);
        q = (base + steps) * v.boxT;
    }
    else
        q = std::round(v.tonnes * 10.0) / 10.0 + steps;
    return std::clamp(q, 0.0, v.maxT);
}

double tradeDealAtFraction(const TradeDealView& v, double fraction)
{
    if (!v.valid)
        return 0.0;
    if (fraction >= 0.995)
        return v.maxT;
    const double raw = std::clamp(fraction, 0.0, 1.0) * v.maxT;
    const double q = v.unitsC20 ? std::round(raw / v.boxT) * v.boxT : std::round(raw * 10.0) / 10.0;
    return std::clamp(q, 0.0, v.maxT);
}
} // namespace game::presentation
