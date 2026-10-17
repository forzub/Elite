#include "src/game/debug/RuntimeScenarioSelection.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using game::debug::RuntimeScenarioKind;
using game::debug::RuntimeScenarioSelection;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testExplicitSelection()
{
    {
        const auto s = RuntimeScenarioSelection::parse("");
        require(!s.active(), "empty scenario unexpectedly active");
    }

    {
        const auto s =
            RuntimeScenarioSelection::parse("ship-control");
        require(
            s.kind == RuntimeScenarioKind::ShipControl,
            "ship-control did not select control oracle"
        );
        require(s.payload.empty(), "ship-control unexpectedly has payload");
    }

    {
        const auto s =
            RuntimeScenarioSelection::parse(
                "ship-damage:Assisted-I@0,Newtonian-Y@1"
            );
        require(
            s.kind == RuntimeScenarioKind::ShipDamage,
            "ship-damage did not select damage oracle"
        );
        require(
            s.payload == "Assisted-I@0,Newtonian-Y@1",
            "ship-damage payload changed"
        );
    }

    {
        const auto s =
            RuntimeScenarioSelection::parse(
                "nav-stress:assisted-high-speed"
            );
        require(
            s.kind == RuntimeScenarioKind::NavigationStress,
            "nav-stress did not select navigation harness"
        );
        require(
            s.payload == "assisted-high-speed",
            "nav-stress payload changed"
        );
    }

    {
        const auto s =
            RuntimeScenarioSelection::parse("something-else");
        require(
            !s.active(),
            "unknown scenario silently became active"
        );
    }
}

} // namespace

int main()
{
    try
    {
        testExplicitSelection();

        std::cout
            << "RUNTIME SCENARIO SELECTION: PASS\n"
            << " - one explicit scenario kind is selected\n"
            << " - payload is preserved for the selected harness\n"
            << " - unknown specs do not silently select another harness\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "RUNTIME SCENARIO SELECTION: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
