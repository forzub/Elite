#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace game::presentation
{
/*
    Right panel of the System Map ("КАРТА СИСТЕМЫ"): answers where am I, what
    is here, what is happening and how to get there - from the world model
    only (world sites runtime copy: hub facts, routes, system summaries built
    from polities / economy / events). Nothing here is a hand-written status.

    Nothing selected -> the system summary (actors, economy, state, relation
    to the player, the hubs one can look for). A hub / site / relay / anchor
    / body selected -> its facts in the same block.
*/
struct SystemMapInfoInput
{
    int systemId = -1;
    const world::celestial::WorldSitesCatalog* catalog = nullptr;
    const world::celestial::SystemMapSnapshot* system = nullptr;
    // overlay object id ("site:<id>", replicated hub id, ship key) or empty
    std::string selectedObjectId;
    std::string selectedBodyId;
    // a selected replicated hub: its name and composition (modules attached
    // to it in the replicated world: "name x count"), empty otherwise
    std::string selectedHubName;
    std::vector<std::string> selectedHubModules;
    // relation of the player to this place: from warrants / reputation when they exist
    std::string playerRelation = "нейтральное: записей, ордеров и претензий нет";
};

namespace detail
{
inline std::string groupDigits(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f", v);
    std::string s = buf;
    std::string out;
    const int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i)
    {
        out += s[static_cast<std::size_t>(i)];
        const int rest = n - 1 - i;
        if (rest > 0 && rest % 3 == 0 && s[static_cast<std::size_t>(i)] != '-')
            out += ' ';
    }
    return out;
}

inline std::string scaled(double v)
{
    char buf[64];
    if (v >= 1.0e12)
        std::snprintf(buf, sizeof(buf), "%.1f трлн", v / 1.0e12);
    else if (v >= 1.0e9)
        std::snprintf(buf, sizeof(buf), "%.2f млрд", v / 1.0e9);
    else if (v >= 1.0e6)
        std::snprintf(buf, sizeof(buf), "%.1f млн", v / 1.0e6);
    else
        return groupDigits(v);
    return buf;
}

inline std::string join(const std::vector<std::string>& v, const char* sep = ", ")
{
    std::string out;
    for (const auto& x : v)
        out += (out.empty() ? "" : sep) + x;
    return out;
}

inline std::string stateText(const std::string& s)
{
    if (s == "quiet") return "спокойное: значимых чрезвычайных событий нет, тихая жизнь";
    if (s == "shortage") return "дефицит отдельных товаров";
    if (s == "epidemic") return "эпидемия / санитарное ограничение";
    if (s == "ship_missing") return "пропал корабль (поиск)";
    if (s == "war") return "война";
    if (s == "quarantine") return "карантин";
    return s;
}

inline void add(std::vector<SystemMapInfoLine>& out, SystemMapInfoLine::Kind k, std::string label, std::string value = {})
{
    out.push_back({k, std::move(label), std::move(value)});
}

inline const world::celestial::WorldSite* siteOf(const SystemMapInfoInput& in, const std::string& objectId)
{
    if (!in.catalog || objectId.empty())
        return nullptr;
    const std::string id = objectId.rfind("site:", 0) == 0 ? objectId.substr(5) : objectId;
    return in.catalog->findSite(in.systemId, id);
}

inline std::string siteName(const SystemMapInfoInput& in, const std::string& id)
{
    if (const auto* s = in.catalog ? in.catalog->findSite(in.systemId, id) : nullptr)
        return s->name;
    return id;
}
}

inline void buildSystemMapInfo(const SystemMapInfoInput& in, std::string& title, std::vector<SystemMapInfoLine>& out)
{
    using K = SystemMapInfoLine::Kind;
    using namespace detail;
    title = "КАРТА СИСТЕМЫ";
    out.clear();
    if (!in.catalog || in.systemId < 0)
        return;

    // ---------------------------------------------------------------- a selected world object
    if (const auto* s = siteOf(in, in.selectedObjectId))
    {
        add(out, K::Heading, s->name);
        add(out, K::Field, "Тип", s->typeLabel.empty() ? s->type : s->typeLabel);
        if (!s->polityName.empty())
            add(out, K::Field, "Владелец", s->polityName);
        if (!s->operatorName.empty())
            add(out, K::Field, "Оператор", s->operatorName);
        if (!s->jurisdiction.empty())
            add(out, K::Field, "Юрисдикция", s->jurisdiction + (s->enforcement.empty() ? "" : " (" + s->enforcement + ")"));
        if (s->population > 0)
            add(out, K::Field, "Население", groupDigits(static_cast<double>(s->population)));
        if (s->founded > 0)
            add(out, K::Field, "Основан", std::to_string(s->founded) + (s->founder.empty() ? "" : ", " + s->founder));
        if (!s->purpose.empty())
            add(out, K::Field, "Назначение", s->purpose);
        if (!s->functions.empty())
            add(out, K::Field, "Функции", s->functions);
        if (!s->roles.empty())
            add(out, K::Field, "Роли", s->roles);
        if (s->docks > 0)
            add(out, K::Field, "Доки", s->dockModules > 0
                ? std::to_string(s->dockSectors) + " сект., " + std::to_string(s->dockModules) + " модулей HexDock, " + std::to_string(s->docks) + " площадок"
                : std::to_string(s->docks) + " площадок");
        if (s->passengerTerminals > 0 || s->cargoTerminals > 0)
            add(out, K::Field, "Терминалы", std::to_string(s->passengerTerminals) + " пасс., " + std::to_string(s->cargoTerminals) + " груз., " +
                                             std::to_string(s->serviceBerths) + " сервисных причалов" +
                                             (s->terminalNames.empty() ? std::string() : "; " + s->terminalNames));
        if (s->arrivalsPerDay > 0)
        {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%s прибытий в сутки, загрузка %.0f %%", groupDigits(s->arrivalsPerDay).c_str(), s->utilisation * 100.0);
            add(out, K::Field, "Трафик", buf);
        }
        if (!s->repair.empty())
            add(out, K::Field, "Ремонт", s->repair);
        if (s->storageTonnes > 0.0)
            add(out, K::Field, "Склады", scaled(s->storageTonnes) + " т");
        if (s->security > 0.0)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f / 100", s->security * 100.0);
            add(out, K::Field, "Безопасность", buf);
        }
        if (!s->anchorName.empty() && s->type != "belt_anchor")
            add(out, K::Field, "Где", "у " + s->anchorName + " (пояс)");
        add(out, K::Field, "Состояние", "нормальное");
        add(out, K::Field, "Отношение к вам", in.playerRelation);
        if (!s->character.empty())
            add(out, K::Paragraph, "", s->character);
        if (!in.selectedHubModules.empty())
        {
            add(out, K::Heading, "Состав");
            for (const auto& m : in.selectedHubModules)
                add(out, K::Bullet, "", m);
        }
        // principal routes through this place (aggregate network)
        std::vector<const world::celestial::WorldRoute*> routes;
        for (const auto& r : in.catalog->routesInSystem(in.systemId))
            if (r.origin == s->id || r.destination == s->id ||
                (!s->runtimeBinding.empty() && (s->runtimeBinding.find("[" + r.origin + "]") != std::string::npos ||
                                                s->runtimeBinding.find("[" + r.destination + "]") != std::string::npos)))
                routes.push_back(&r);
        std::sort(routes.begin(), routes.end(), [](const auto* a, const auto* b) { return a->flightsPerDay > b->flightsPerDay; });
        if (!routes.empty())
        {
            add(out, K::Heading, "Маршруты");
            for (std::size_t i = 0; i < routes.size() && i < 8; ++i)
            {
                const auto* r = routes[i];
                const std::string other = r->origin == s->id || s->runtimeBinding.find("[" + r->origin + "]") != std::string::npos ? r->destination : r->origin;
                char buf[160];
                std::snprintf(buf, sizeof(buf), " — %.1f рейс./сут, %s, в пути %.1f сут", r->flightsPerDay, r->shipClass.c_str(), r->travelDays);
                add(out, K::Bullet, "", siteName(in, other) + buf + (r->disruption != "none" && !r->disruption.empty() ? ", сбой: " + r->disruption : ""));
            }
        }
        return;
    }

    // ---------------------------------------------------------------- a replicated hub without world-site facts
    if (!in.selectedHubName.empty())
    {
        add(out, K::Heading, in.selectedHubName);
        add(out, K::Field, "Тип", "орбитальный хаб");
        if (!in.selectedHubModules.empty())
        {
            add(out, K::Heading, "Состав");
            for (const auto& m : in.selectedHubModules)
                add(out, K::Bullet, "", m);
        }
        add(out, K::Field, "Отношение к вам", in.playerRelation);
        return;
    }

    // ---------------------------------------------------------------- a selected body
    if (in.system && !in.selectedBodyId.empty())
        for (const auto& b : in.system->bodies)
            if (b.id == in.selectedBodyId)
            {
                add(out, K::Heading, b.name);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%.0f км", b.radiusKm);
                add(out, K::Field, "Радиус", buf);
                std::snprintf(buf, sizeof(buf), "%.3f а.е.", std::sqrt(b.positionAu.x * b.positionAu.x + b.positionAu.y * b.positionAu.y + b.positionAu.z * b.positionAu.z));
                add(out, K::Field, "От звезды", buf);
                // major hubs first (what the map shows); settlements and local facilities only counted
                std::vector<std::string> major;
                int local = 0, relays = 0;
                for (const auto& s : in.catalog->sitesInSystem(in.systemId))
                {
                    const bool here = s.parentBodyId == b.id || s.lagrangeSecondaryId == b.id ||
                                      (!s.parentBodyId.empty() && s.parentBodyId.rfind(b.id + ".", 0) == 0);
                    if (!here)
                        continue;
                    if (s.type == "nav_relay")
                        ++relays;
                    else if (s.level == "A" && s.mapLevel == "system")
                        major.push_back(s.name + (s.roles.empty() ? (s.typeLabel.empty() ? "" : " — " + s.typeLabel) : " — " + s.roles));
                    else
                        ++local;
                }
                if (!major.empty())
                {
                    add(out, K::Heading, "Крупные хабы (" + std::to_string(major.size()) + ")");
                    for (const auto& m : major)
                        add(out, K::Bullet, "", m);
                }
                if (local > 0)
                    add(out, K::Field, "Поселения и местные объекты", std::to_string(local) + " — на Detail Map (P)");
                if (relays > 0)
                    add(out, K::Field, "Навигационные ретрансляторы", std::to_string(relays) + " (фильтр N)");
                return;
            }

    // ---------------------------------------------------------------- the system summary
    const auto* w = in.catalog->summary(in.systemId);
    if (!w)
    {
        add(out, K::Paragraph, "", "Сведений о населённых местах этой системы нет.");
        return;
    }
    add(out, K::Heading, "Система " + w->name);
    add(out, K::Paragraph, "", w->primaryName + ". Население " + scaled(w->population) + ", выпуск " + scaled(w->gdpPerYear) + " Cr в год, "
                               "около " + groupDigits(w->trafficArrivalsPerDay) + " межзвёздных прибытий в сутки.");
    add(out, K::Heading, "Главные политические акторы");
    add(out, K::Bullet, "", w->primaryName + " — основная власть");
    for (const auto& a : w->otherActors)
        add(out, K::Bullet, "", a);
    add(out, K::Heading, "Экономика");
    add(out, K::Field, "Население", scaled(w->population));
    add(out, K::Field, "Выпуск", scaled(w->gdpPerYear) + " Cr/год");
    if (!w->industries.empty())
        add(out, K::Field, "Главные отрасли", join(w->industries));
    if (!w->imports.empty())
        add(out, K::Field, "Импорт", join(w->imports));
    if (!w->exports.empty())
        add(out, K::Field, "Экспорт", join(w->exports));
    if (!w->shortages.empty())
        add(out, K::Field, "Дефицит", join(w->shortages));
    add(out, K::Heading, "Отношение к вам");
    add(out, K::Paragraph, "", in.playerRelation);
    add(out, K::Heading, "Текущее состояние");
    for (const auto& s : w->state)
        add(out, K::Bullet, "", stateText(s));
    for (const auto& h : w->hazards)
        add(out, K::Bullet, "", "опасная зона: " + h);
    // discoverability: what one can look for on this map, by name and by region (no ids, no cubes);
    // every region appears, not only the busiest one
    struct Group { std::string label; std::vector<const world::celestial::WorldSite*> hubs; double traffic = 0.0; };
    std::vector<Group> groups;
    auto regionLabel = [&](const world::celestial::WorldSite& s) {
        std::string id = !s.parentBodyId.empty() ? s.parentBodyId : s.lagrangeSecondaryId;
        if (s.placement == world::celestial::WorldSitePlacementKind::Heliocentric || id.find("asteroid_belt") != std::string::npos)
            return std::string("Главный пояс");
        if (in.system)
            for (const auto& b : in.system->bodies)
                if (b.id == id)
                    return b.name;
        const std::size_t dot = id.rfind('.');
        return dot == std::string::npos ? id : id.substr(dot + 1);
    };
    std::size_t total = 0;
    for (const auto& s : in.catalog->sitesInSystem(in.systemId))
    {
        if (s.type == "nav_relay" || s.type == "belt_anchor" || s.mapLevel != "system")
            continue;
        const std::string label = regionLabel(s);
        auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) { return x.label == label; });
        if (g == groups.end())
        {
            groups.push_back({label, {}, 0.0});
            g = groups.end() - 1;
        }
        g->hubs.push_back(&s);
        g->traffic += s.arrivalsPerDay;
        ++total;
    }
    std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) { return a.traffic > b.traffic; });
    if (total)
    {
        add(out, K::Heading, "Хабы по районам (" + std::to_string(total) + ")");
        for (auto& g : groups)
        {
            std::sort(g.hubs.begin(), g.hubs.end(), [](const auto* a, const auto* b) { return a->arrivalsPerDay > b->arrivalsPerDay; });
            std::string names;
            for (std::size_t i = 0; i < g.hubs.size() && i < 4; ++i)
                names += (i ? ", " : "") + g.hubs[i]->name;
            if (g.hubs.size() > 4)
                names += " и ещё " + std::to_string(g.hubs.size() - 4);
            add(out, K::Bullet, "", g.label + ": " + names);
        }
    }
    add(out, K::Paragraph, "", "Наведите курсор на значок — имя и владелец; щелчок — подробности здесь. Все хабы и маяки — в списке внизу. G — сетка кубов, T — маршруты, N — маяки.");
}

// Marker id of a world site on the System Map: the replicated hub id for a site bound to a
// runtime hub ("initial_world_state.orbital_hubs[<id>]"), else "site:<id>" (ClientWorldSitesMapBridge).
inline std::string systemMapObjectIdOf(const world::celestial::WorldSite& s)
{
    const auto open = s.runtimeBinding.rfind('[');
    const auto close = s.runtimeBinding.rfind(']');
    if (open != std::string::npos && close != std::string::npos && close > open + 1)
        return s.runtimeBinding.substr(open + 1, close - open - 1);
    return "site:" + s.id;
}

/*
    The full list of what the System Map draws as hubs and beacons (the panel list that replaces
    the star-system list on the System / Detail map): hubs grouped by planet (moons, Lagrange
    points with their planet; the belt as one group) in order of distance from the star, then the
    beacons. Same filter as the map bridge - unknown objects (map level "none") are not listed.
*/
inline std::vector<SystemMapPanelObjectItem> buildSystemMapObjectList(const SystemMapInfoInput& in)
{
    std::vector<SystemMapPanelObjectItem> out;
    if (!in.catalog || in.systemId < 0)
        return out;
    struct Group { std::string label; double radiusAu = 1.0e9; std::vector<const world::celestial::WorldSite*> sites; };
    std::vector<Group> groups;
    std::vector<const world::celestial::WorldSite*> beacons;
    glm::dvec3 starAu(0.0);
    if (in.system)
        for (const auto& b : in.system->bodies)
            if (b.type == world::celestial::BodyType::Star)
            {
                starAu = b.positionAu;
                break;
            }
    for (const auto& s : in.catalog->sitesInSystem(in.systemId))
    {
        if (s.type == "belt_anchor" || (s.mapLevel != "system" && s.runtimeBinding.empty()))
            continue;
        if (s.type == "nav_relay")
        {
            beacons.push_back(&s);
            continue;
        }
        // the planet: third segment of "system_N.Star.Planet[.Moon]"
        const std::string id = !s.parentBodyId.empty() ? s.parentBodyId : s.lagrangeSecondaryId;
        std::string planetId = id;
        double radiusAu = 1.0e9;
        std::string label;
        if (s.placement == world::celestial::WorldSitePlacementKind::Heliocentric || id.find("asteroid_belt") != std::string::npos)
        {
            label = "Главный пояс";
            radiusAu = in.system ? 2.7 : radiusAu;     // without bodies: catalog order
        }
        else
        {
            std::size_t dot = id.find('.');
            dot = dot == std::string::npos ? dot : id.find('.', dot + 1);
            dot = dot == std::string::npos ? dot : id.find('.', dot + 1);
            if (dot != std::string::npos)
                planetId = id.substr(0, dot);
            const std::size_t last = planetId.rfind('.');
            label = last == std::string::npos ? planetId : planetId.substr(last + 1);
            if (in.system)
                for (const auto& b : in.system->bodies)
                    if (b.id == planetId)
                    {
                        label = b.name;
                        radiusAu = glm::length(b.positionAu - starAu);
                    }
        }
        auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) { return x.label == label; });
        if (g == groups.end())
        {
            groups.push_back({label, radiusAu, {}});
            g = groups.end() - 1;
        }
        g->sites.push_back(&s);
    }
    std::stable_sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) { return a.radiusAu < b.radiusAu; });
    auto push = [&](const world::celestial::WorldSite& s) {
        SystemMapPanelObjectItem item;
        item.objectId = systemMapObjectIdOf(s);
        item.label = s.name + (s.type == "nav_relay" || s.typeLabel.empty() ? std::string() : " — " + s.typeLabel);
        item.selected = !in.selectedObjectId.empty() && item.objectId == in.selectedObjectId;
        out.push_back(std::move(item));
    };
    for (const auto& g : groups)
    {
        out.push_back({std::string(), g.label + " (" + std::to_string(g.sites.size()) + ")", true, false});
        for (const auto* s : g.sites)
            push(*s);
    }
    if (!beacons.empty())
    {
        out.push_back({std::string(), "Маяки (" + std::to_string(beacons.size()) + ")", true, false});
        for (const auto* s : beacons)
            push(*s);
    }
    return out;
}
}
