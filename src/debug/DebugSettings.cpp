#include "DebugSettings.h"

namespace debug
{

DebugSettings& get()
{
    static DebugSettings instance;
    return instance;
}

} // namespace debug