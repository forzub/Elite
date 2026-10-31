#pragma once

#include <string>
#include <vector>

#include "tools/planet_lab/PlanetLabBodies.h"

namespace planet_lab
{
// Interactive viewer; with captureDir set, renders every body at every
// viewpoint to PNG files and exits.
int runViewer(const std::vector<LabBody>& bodies, const std::string& captureDir);
} // namespace planet_lab
