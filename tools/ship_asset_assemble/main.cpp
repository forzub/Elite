// EliteShipAssetAssemble <source dir with ship_asset.json> <production.elmodel> [<working.elmodel>]
// (the work itself: ShipAssetAssemble.cpp)
#include <cstdio>
#include <string>

#include "tools/ship_asset_assemble/ShipAssetAssemble.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: EliteShipAssetAssemble <source dir> <production.elmodel> [<working.elmodel>]\n");
        return 2;
    }
    std::string report;
    const bool ok = elite::ship_asset::assembleShipAsset(argv[1], argv[2], argc > 3 ? argv[3] : std::string(), report);
    std::fprintf(ok ? stdout : stderr, "%s\n", report.c_str());
    return ok ? 0 : 1;
}
