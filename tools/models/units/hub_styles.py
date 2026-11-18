"""The national STYLE kits of the hub cities (author 2026-11-18: «сгенерируй модули для хабов для других политических систем
согласно их духу ... главное, что бы по прибытию все эти хабы визуально различались, но что бы имели одинаковый слой
механики»): every style kit replaces the downtown / residential / hotel modules of the planar city generator
(hub_plane.ZONE_MODULES); the mechanics layer (streets, lanes, docks, services, power, schedule) stays the same.

Kits: units.hub_style_east (ultramodern, ancient, indian), units.hub_style_west (practical, belt_clans, american); each
exports STYLE_ZONE_MODULES and STYLE_POLITIES. Sol keeps the default elegant kit (style "").

Landmarks (every hub recognizable by its own one-offs, any style): units.hub_landmarks - the hubl_* modules, their
placement table LANDMARKS and the picker landmarks_for(hub_id, style) (re-exported here; how hub_plane should apply
it: that module's docstring).
"""
import importlib

UNITS = ("hub_style_east", "hub_style_west")


def _tables():
    zones, polities = {}, {}
    for unit in UNITS:
        try:
            mod = importlib.import_module(f"units.{unit}")
        except ImportError:
            continue
        zones.update(getattr(mod, "STYLE_ZONE_MODULES", {}))
        for style, ps in getattr(mod, "STYLE_POLITIES", {}).items():
            for p in ps:
                polities.setdefault(p, style)          # a polity claimed twice: the first kit wins
    return zones, polities


def style_zone_modules(style):
    return _tables()[0].get(style, {})


def styles():
    return sorted(_tables()[0])


def polity_style(polity):
    """the style of a polity (its founding culture); "" = the default kit"""
    return _tables()[1].get(polity, "")


def landmarks_for(hub_id, style=None):
    """the hub's 2-4 landmarks + its block-layout signature (units.hub_landmarks.landmarks_for)"""
    from units.hub_landmarks import landmarks_for as pick
    return pick(hub_id, style)


def landmark_info(module):
    """kind / place / span / title of a landmark module (None: not a landmark)"""
    from units.hub_landmarks import LANDMARKS
    return LANDMARKS.get(module)
