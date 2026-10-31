#pragma once

#include <string>
#include <vector>

#include "tools/planet_lab/PlanetLabBodies.h"

namespace planet_lab
{
/*
    Reproducibility and LOD-consistency checks on the production generator
    (no rendering). Each check measures a real property:

      record / verify  checksums of patches (macro + full), landmarks,
                       climate samples, clouds at fixed world times and the
                       asteroid identity -> a file; a separate process
                       verifies the same values (restart reproducibility).
      order            patches requested in different orders and on worker
                       threads give identical patch checksums.
      parent_child     at coincident vertices a child keeps the parent's
                       water flag, landmark id and macro height; heights
                       differ at most by the detail bound.
      coastline        water / landmark / macro height of 20000 directions
                       identical from 50 km down to 0.25 m band limit; land
                       stays above the water it borders.
      seams            neighbouring patches (incl. across cube faces) share
                       edge positions; coarse/fine T-junction gaps are
                       covered by the skirts.
      climate          polar ice > tropical ice, desert peak in the
                       subtropics, biomes vary along a latitude.
      clouds           same time -> same sky; weather moves with time.
      policy           gas giants: no solid ground; crushing / molten
                       surfaces: no ground patches.
      asteroid         the silhouette (macro radius) is LOD-independent.

    Returns the number of failed checks.
*/
int runLabChecks(const std::vector<LabBody>& bodies, const std::string& recordPath, const std::string& verifyPath);
} // namespace planet_lab
