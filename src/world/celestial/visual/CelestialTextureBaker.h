#pragma once

#include <string>

namespace world::celestial::visual
{

struct CelestialTextureBakeOptions
{
    std::string presetsPath =
        "src/assets/data/celestial/visual_presets.json";

    std::string bodyVisualsRoot =
        "src/assets/data/celestial/body_visuals";

    std::string outputRoot =
        "assets/generated/celestial";

    // Optional filters.
    //
    // Examples:
    //   systemFilter = "sol"
    //   bodyFilter   = "earth"
    //
    // bodyFilter may also be compound:
    //   "sol/earth"
    //
    std::string systemFilter;
    std::string bodyFilter;

    // Diagnostic mode:
    // print matching bodies, do not bake.
    bool listOnly = false;

    // If true, ImportedRealData bodies are printed as skipped
    // instead of being silently ignored.
    bool printSkipped = true;

    // Future option:
    // once ImportedRealData importer exists, this can enable/disable it.
    bool allowImportedRealDataBake = false;

    // Check source_catalog required assets and print missing files.
    // Does not bake.
    bool checkSources = false;
};

class CelestialTextureBaker
{
public:
    bool bakeLibrary(
        const CelestialTextureBakeOptions& options
    );

    // Backward-compatible wrapper.
    bool bakeLibrary(
        const std::string& presetsPath,
        const std::string& bodyVisualsRoot,
        const std::string& outputRoot
    );
};

} // namespace world::celestial::visual