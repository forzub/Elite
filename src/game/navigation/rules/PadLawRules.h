#pragma once

// CITY PADS AND THE NEWTONIAN LAW (author 2026-11-17: traffic classes - local streets and avenues are Assisted,
// «Newtonian / larger ships never enter - все доки должны быть на окраинах»; 2026-11-18: the frontiers' docks by the
// storage / logistics blocks on the rim; 2026-11-26: «у нас есть специальные порты для ньютона ... потому, что ньютону
// тяжеловато маневрировать в городе» -> the dispatcher REFUSES a city pad to a Newtonian ship and sends it to the
// outskirts: a ring hub's axial dock (on the axis the deck stands still), a planar city's rim piers (the plan's
// role "dock" modules - not taking ships yet) and, today, the city's own dock: moved to a rim block 2026-11-17, its slot
// facing open space - live 2026-11-26 a Newtonian Cobra docked at Tikhiy's and at the ring's axial dock).
// One source for the client's dispatcher (at the click, before any route) and the server's (at the request).

#include <string>

namespace game::navigation::rules
{

// a city pad (roof, stilts, garage, repair hangar - every pad of a hub plan) is open to the ship's control family
inline bool cityPadOpenTo(bool newtonianFamily)
{
    return !newtonianFamily;
}

// the dispatcher's notice: "refused:newtonian:<address>:<ring 0|1>"
inline std::string newtonianPadNotice(const std::string& address, bool ringHub)
{
    return "refused:newtonian:" + address + ":" + (ringHub ? "1" : "0");
}

// the pilot's line for that notice (empty: not this notice)
inline std::string newtonianPadLine(const std::string& status)
{
    const std::string head = "refused:newtonian:";
    if (status.rfind(head, 0) != 0)
        return {};
    const std::string rest = status.substr(head.size());
    const auto colon = rest.rfind(':');
    const std::string address = colon == std::string::npos ? rest : rest.substr(0, colon);
    const bool ring = colon != std::string::npos && rest.substr(colon + 1) == "1";
    return "ДИСПЕТЧЕР: посадка на " + address +
           " отклонена - городские площадки закрыты для ньютоновских кораблей (в городе им не развернуться). " +
           (ring ? std::string("Стыкуйтесь в осевой док (Док) - на оси палуба стоит")
                 : std::string("Стыкуйтесь в док города (Док) - он на окраине"));
}

} // namespace game::navigation::rules
