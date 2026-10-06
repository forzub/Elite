#include "src/game/navigation/autopilot/CourseLeadGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Guidance =
    game::navigation::autopilot::CourseLeadGuidance;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testLowSpeedKeepsMinimumPreview()
{
    Guidance::Request request;
    request.actualSpeedMps = 12.0;
    request.courseResponseSeconds = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "low-speed course lead invalid");
    require(std::abs(out.lookAheadMeters - 50.0) < 1.0e-12,
        "low-speed course lead fell below proven 50 m preview");
}

void testMeasuredResponseExpandsPreviewInsideBounds()
{
    Guidance::Request request;
    request.actualSpeedMps = 80.0;
    request.courseResponseSeconds = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "measured-response course lead invalid");
    require(std::abs(out.lookAheadMeters - 160.0) < 1.0e-12,
        "measured course response was not reflected in route preview");
}

void testHighSpeedCannotCreateLongChordPreview()
{
    Guidance::Request request;
    request.actualSpeedMps = 288.0;
    request.courseResponseSeconds = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "high-speed course lead invalid");
    require(std::abs(out.lookAheadMeters - 250.0) < 1.0e-12,
        "high-speed course lead exceeded proven 250 m cap");
}

}

int main()
{
    try
    {
        testLowSpeedKeepsMinimumPreview();
        testMeasuredResponseExpandsPreviewInsideBounds();
        testHighSpeedCannotCreateLongChordPreview();

        std::cout
            << "COURSE LEAD GUIDANCE TESTS: PASS\n"
            << " - low-speed preview is at least 50 m\n"
            << " - measured Assisted response expands preview\n"
            << " - high-speed preview is capped at 250 m\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "COURSE LEAD GUIDANCE TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
