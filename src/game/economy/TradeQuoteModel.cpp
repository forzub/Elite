#include "src/game/economy/TradeQuoteModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game::economy
{
double bidCr(const MarketSide& m)
{
    return m.priceCr * (1.0 - 0.5 * m.spread);
}

double askCr(const MarketSide& m)
{
    return m.priceCr * (1.0 + 0.5 * m.spread);
}

double hubTakesUpToT(const MarketSide& m, const TradeDealRules& r)
{
    return std::max(0.0, r.hubBuyCapShare * m.targetT - m.stockT);
}

double hubGivesUpToT(const MarketSide& m, const TradeDealRules& r)
{
    return std::max(0.0, m.stockT - r.minStockShare * m.targetT);
}

double sellUnitCr(const MarketSide& m, double tonnes, const TradeDealRules& r)
{
    const double s = std::max(m.stockT, r.minStockShare * m.targetT);
    if (!(s > 0.0))
        return bidCr(m);
    return bidCr(m) * std::pow(s / (s + r.kappa * std::max(0.0, tonnes) * 0.5), r.stockExponent);
}

double buyUnitCr(const MarketSide& m, double tonnes, const TradeDealRules& r)
{
    const double s = std::max(m.stockT, 1.0e-9);
    const double after = std::max(s - r.kappa * std::max(0.0, tonnes) * 0.5, std::max(r.minStockShare * m.targetT, 1.0e-9));
    return askCr(m) * std::pow(s / std::min(s, after), r.stockExponent);
}

double dockingFeeCr(double dryMassT, const FiscalStyle& s, const TradeDealRules& r)
{
    return (r.dockingBaseCr + r.dockingCrPerDryT * std::max(0.0, dryMassT)) * s.portIndex;
}

namespace
{
std::string pct(double share)
{
    char b[32];
    std::snprintf(b, sizeof b, "%.0f %%", share * 100.0);
    return b;
}
} // namespace

DealQuote explainDeal(bool buy, const MarketSide& m, double tonnes, const FiscalStyle& style, const TradeDealRules& r)
{
    DealQuote q;
    q.buy = buy;
    q.tonnes = tonnes;
    if (!(tonnes > 0.0))
    {
        q.code = "zero";
        q.reason = "нулевое количество";
        return q;
    }
    q.listUnitCr = buy ? askCr(m) : bidCr(m);
    q.unitCr = buy ? buyUnitCr(m, tonnes, r) : sellUnitCr(m, tonnes, r);
    q.impactShare = q.listUnitCr > 0.0 ? q.unitCr / q.listUnitCr - 1.0 : 0.0;
    q.goodsCr = tonnes * q.unitCr;
    q.handlingCr = tonnes * r.handlingCrPerT * style.portIndex;
    char label[96];
    if (buy)
    {
        q.vatCr = style.vatShare * (q.goodsCr + q.handlingCr);
        q.vatReclaimCr = style.vatShare * q.goodsCr;
        q.lines.push_back({"товар", -q.goodsCr});
        std::snprintf(label, sizeof label, "погрузка %.1f т", tonnes);
        q.lines.push_back({label, -q.handlingCr});
        if (q.vatCr > 0.0)
        {
            std::snprintf(label, sizeof label, "НДС %s (с товара — к возврату через %d сут)", pct(style.vatShare).c_str(),
                          style.vatRefundDays);
            q.lines.push_back({label, -q.vatCr});
        }
    }
    else
    {
        q.vatCr = style.vatShare * q.handlingCr;
        q.turnoverCr = style.turnoverShare * q.goodsCr;
        q.lines.push_back({"выручка", q.goodsCr});
        std::snprintf(label, sizeof label, "выгрузка %.1f т", tonnes);
        q.lines.push_back({label, -q.handlingCr});
        if (q.vatCr > 0.0)
            q.lines.push_back({"НДС с портовой услуги " + pct(style.vatShare), -q.vatCr});
        if (q.turnoverCr > 0.0)
            q.lines.push_back({(style.turnoverRu.empty() ? std::string("налог с оборота") : style.turnoverRu) + " " +
                                   pct(style.turnoverShare),
                               -q.turnoverCr});
    }
    for (const auto& l : q.lines)
        q.totalCr += l.cr;

    const double cap = buy ? hubGivesUpToT(m, r) : hubTakesUpToT(m, r);
    if (tonnes > cap + 1.0e-9)
    {
        q.code = buy ? "hub_empty" : "hub_full";
        char b[128];
        std::snprintf(b, sizeof b, buy ? "хаб отпускает не больше %.1f т" : "склад хаба полон: берёт не больше %.1f т", cap);
        q.reason = b;
        return q;
    }
    q.ok = true;
    return q;
}
} // namespace game::economy
