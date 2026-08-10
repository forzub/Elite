#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "src/debug/DebugSettings.h"
#include "src/game/debug/DebugControlSettingsCodec.h"
#include "src/ui/html/HtmlUiMessage.h"

namespace
{
using json = nlohmann::json;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testFullSettingsRoundTrip()
{
    debug::DebugRenderSettings source;
    json expected = game::debug_control::encodeRenderSettings(source);

    for (auto it = expected.begin(); it != expected.end(); ++it)
    {
        if (it->is_boolean())
        {
            *it = !it->get<bool>();
        }
        else if (it->is_number_integer())
        {
            *it = 1;
        }
        else if (it->is_number_float())
        {
            // Keep the round-trip fixture inside the authoritative legal
            // ranges. Clamp behaviour is exercised separately below.
            const std::string key = it.key();
            *it = key == "postGrain" ? 0.05f : 0.75f;
        }
        else if (it->is_string())
        {
            *it = "promo1";
        }
        else if (it->is_array())
        {
            *it = json::array({0.15f, 0.25f, 0.35f, 0.45f});
        }
    }

    debug::DebugRenderSettings target;
    game::debug_control::applyRenderSettings(target, expected);

    const json actual =
        game::debug_control::encodeRenderSettings(target);

    require(
        actual.size() == expected.size(),
        "debug settings JSON round-trip changed key count"
    );

    for (auto it = expected.begin(); it != expected.end(); ++it)
    {
        const auto actualIt = actual.find(it.key());
        require(
            actualIt != actual.end(),
            std::string("debug settings JSON round-trip lost key: ") + it.key()
        );
        require(
            *actualIt == *it,
            std::string("debug settings JSON round-trip changed key: ") + it.key() +
                " expected=" + it->dump() +
                " actual=" + actualIt->dump()
        );
    }
}

void testPartialPayloadPreservesOtherSettings()
{
    debug::DebugRenderSettings settings;
    settings.renderHubs = false;
    settings.postContrast = 1.23f;

    game::debug_control::applyRenderSettings(
        settings,
        json{{"renderNpcShips", false}}
    );

    require(!settings.renderNpcShips, "partial payload did not change requested field");
    require(!settings.renderHubs, "partial payload reset unrelated bool field");
    require(
        settings.postContrast == 1.23f,
        "partial payload reset unrelated numeric field"
    );
}

void testClampsRemainServerAuthoritative()
{
    debug::DebugRenderSettings settings;

    game::debug_control::applyRenderSettings(
        settings,
        json{
            {"postBloomThreshold", 99.0},
            {"postBloomKnee", -1.0},
            {"postGrain", 3.0},
            {"systemMapLiveRefreshSec", 0.0},
            {"cloudSpeedMultiplier", -100.0},
            {"sceneMode", "invalid-mode"}
        }
    );

    require(settings.postBloomThreshold == 2.0f, "bloom threshold clamp lost");
    require(settings.postBloomKnee == 0.001f, "bloom knee clamp lost");
    require(settings.postGrain == 0.10f, "grain clamp lost");
    require(settings.systemMapLiveRefreshSec == 0.02f, "map refresh clamp lost");
    require(settings.cloudSpeedMultiplier == 0.0f, "cloud speed clamp lost");
    require(settings.sceneMode == "game", "invalid scene mode was not rejected");
}

void testInternalDiagnosticsAreNotPersistedAsUserSettings()
{
    debug::DebugRenderSettings settings;
    const json payload = game::debug_control::encodeRenderSettings(settings);

    require(!payload.contains("drawSeamDebug"), "internal seam debug leaked into UI contract");
    require(!payload.contains("captureSeamDebug"), "internal capture flag leaked into UI contract");
    require(!payload.contains("seamDebugProxies"), "runtime seam data leaked into UI contract");
}

void testDiagnosticPanelMessageRoutes()
{
    const std::pair<const char*, HtmlUiPanelId> panels[] = {
        {"debug_control", HtmlUiPanelId::DebugControl},
        {"attachment_editor", HtmlUiPanelId::AttachmentEditor},
        {"structure_debug", HtmlUiPanelId::StructureDebug},
        {"volume_viewer", HtmlUiPanelId::VolumeViewer},
        {"ship_core", HtmlUiPanelId::ShipCore},
        {"frustum_debug", HtmlUiPanelId::FrustumDebug},
        {"system_map", HtmlUiPanelId::SystemMap},
    };

    for (const auto& [wireName, expectedPanel] : panels)
    {
        const HtmlUiMessage parsed = HtmlUiMessage::fromJson(
            json{
                {"type", "subscribe"},
                {"panel", wireName},
                {"payload", json::object()}
            }
        );

        require(parsed.type == HtmlUiMessageType::Subscribe, "subscribe message type route broke");
        require(parsed.panel == expectedPanel, std::string("panel route broke: ") + wireName);
        require(toString(parsed.panel) == std::string_view(wireName), std::string("panel serialization broke: ") + wireName);
    }
}

} // namespace

int main()
{
    try
    {
        testFullSettingsRoundTrip();
        testPartialPayloadPreservesOtherSettings();
        testClampsRemainServerAuthoritative();
        testInternalDiagnosticsAreNotPersistedAsUserSettings();
        testDiagnosticPanelMessageRoutes();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] feature contract: " << e.what() << '\n';
        return 1;
    }

    std::cout << "[PASS] debug-control settings round-trip contract\n";
    return 0;
}
