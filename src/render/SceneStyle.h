#pragma once

namespace render
{
/*
    Presentation style of the whole 3D scene (ships, stations, large
    objects, starfield, Milky Way, celestial bodies): 0 = Elite Classic,
    1 = Modern, 2 = Anime. Same values as
    render::celestial_flight::CelestialRenderStyle; the user setting is
    ClientModeState::celestialRenderStyleId. Shaders read it as
    `uniform int uSceneStyle`; the HUD keeps its own look.
*/
inline int& sceneStyleStorage()
{
    static int style = 1;
    return style;
}

inline int sceneStyle()
{
    return sceneStyleStorage();
}

inline void setSceneStyle(int style)
{
    sceneStyleStorage() = style;
}

// The style of the flight scene presented this frame (not reset after the
// scene pass like sceneStyle()): the cinematic composite reads it.
inline int& presentedSceneStyleStorage()
{
    static int style = 1;
    return style;
}

inline int presentedSceneStyle()
{
    return presentedSceneStyleStorage();
}

inline void setPresentedSceneStyle(int style)
{
    presentedSceneStyleStorage() = style;
}
} // namespace render
