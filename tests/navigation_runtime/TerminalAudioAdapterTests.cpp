#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/audio/TerminalAudioAdapter.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        audio::TerminalAudioAdapter adapter;

        require(
            !adapter.shouldPlayTick(0, 0.0),
            "adapter played with zero typed characters"
        );
        require(
            adapter.shouldPlayTick(1, 0.0),
            "adapter did not play first typed character"
        );
        require(
            !adapter.shouldPlayTick(1, 0.01),
            "adapter ignored minimum tick interval"
        );
        require(
            adapter.shouldPlayTick(3, 0.05),
            "adapter failed to emit after minimum interval"
        );

        adapter.reset();
        require(
            adapter.shouldPlayTick(1, 0.0),
            "adapter reset did not restore first-tick behavior"
        );

        std::cout << "TERMINAL AUDIO ADAPTER: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "TERMINAL AUDIO ADAPTER: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
