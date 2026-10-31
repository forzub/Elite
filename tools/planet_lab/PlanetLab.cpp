// Planet Lab: visual proof and reproducibility checks of the procedural
// surface generator (src/world/celestial/surface).
//
//   ElitePlanetLab                      interactive viewer (run from the repo root)
//   ElitePlanetLab --check [--record F] [--verify F]
//                                       headless checks; --record writes the
//                                       checksums, --verify compares them with
//                                       a previous process
//   ElitePlanetLab --capture DIR        renders every body at every viewpoint
//                                       to DIR/planet_lab_<body>_<view>.png
//   ElitePlanetLab --body PATH          adds a generated body JSON

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "tools/planet_lab/PlanetLabBodies.h"
#include "tools/planet_lab/PlanetLabChecks.h"
#include "tools/planet_lab/PlanetLabViewer.h"

int main(int argc, char** argv)
{
    bool checks = false;
    std::string record;
    std::string verify;
    std::string capture;
    std::string root = ".";
    std::vector<std::string> extra;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--check")
            checks = true;
        else if (a == "--record" && i + 1 < argc)
            record = argv[++i];
        else if (a == "--verify" && i + 1 < argc)
            verify = argv[++i];
        else if (a == "--capture" && i + 1 < argc)
            capture = argv[++i];
        else if (a == "--root" && i + 1 < argc)
            root = argv[++i];
        else if (a == "--body" && i + 1 < argc)
            extra.push_back(argv[++i]);
    }

    std::vector<planet_lab::LabBody> bodies;
    std::string error;
    if (!planet_lab::loadLabBodies(root, bodies, error))
    {
        std::fprintf(stderr, "Planet Lab: %s (run from the repository root or pass --root)\n", error.c_str());
        return 2;
    }
    for (const std::string& path : extra)
    {
        planet_lab::LabBody body;
        if (!planet_lab::loadLabBodyFromFile(path, body, error))
        {
            std::fprintf(stderr, "Planet Lab: %s\n", error.c_str());
            return 2;
        }
        bodies.push_back(std::move(body));
    }
    for (const auto& b : bodies)
        std::printf("body %-10s r=%.0f km  %s  focus %s\n", b.label.c_str(), b.field->referenceRadiusM() / 1000.0,
                    b.isSmallBody ? "small_body" : planet_lab::archetypeName(planet_lab::surfaceArchetype(b.descriptor)),
                    b.focusLabel.c_str());

    // --probe: the height profile along 200 km eastward from each focus.
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--probe")
        {
            for (const auto& b : bodies)
            {
                const glm::dvec3 up = b.focusDir;
                const glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), up));
                std::printf("%s:", b.label.c_str());
                for (int k = 0; k <= 40; ++k)
                {
                    const glm::dvec3 d = glm::normalize(up + east * (k * 5000.0 / b.field->referenceRadiusM()));
                    std::printf(" %.0f", b.field->sample(d, 100.0).heightM);
                }
                std::printf("\n");
            }
            return 0;
        }

    if (checks || !record.empty() || !verify.empty())
        return planet_lab::runLabChecks(bodies, record, verify) == 0 ? 0 : 1;
    return planet_lab::runViewer(bodies, capture);
}
