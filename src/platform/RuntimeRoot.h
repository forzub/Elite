#pragma once

#include <string>

namespace platform
{
/*
    Establish the executable directory as the process runtime root.

    Runtime assets are staged next to each built executable under
    <executable-dir>/assets.  The process must therefore not depend on the
    shell's launch working directory.  Call this once at process entry before
    any catalog, shader, localization or initial-world data is loaded.
*/
bool initializeExecutableRuntimeRoot(std::string* outError = nullptr);
}
