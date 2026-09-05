#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#include <webview/webview.h>

#include "src/ui/html/HtmlUiServer.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "tools/model_asset_editor/ModelAssetEditorSession.h"
#include "tools/model_asset_editor/EditorVersion.h"

#ifndef ELITE_SOURCE_ROOT
#define ELITE_SOURCE_ROOT "."
#endif

#ifndef ELITE_EDITOR_RUNTIME_ROOT
#define ELITE_EDITOR_RUNTIME_ROOT "."
#endif

int main()
{
    std::error_code runtimeRootError;
    std::filesystem::current_path(std::filesystem::path(ELITE_EDITOR_RUNTIME_ROOT), runtimeRootError);
    if (runtimeRootError)
    {
        std::cerr << "[ModelAssetEditor] runtime root error: " << runtimeRootError.message() << '\n';
        return 1;
    }

    try
    {
        ObjectDescriptorRegistry::ensureInitialized();
        game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();

        HtmlUiServer server;
        const std::uint16_t port = server.start(
            0,
            "assets/webui",
            "assets/ui/model_asset_editor_ui.pak");
        elite::model_asset::editor::ModelAssetEditorSession session(
            std::filesystem::path(ELITE_SOURCE_ROOT),
            server
        );

        webview::webview editor(false, nullptr);
        editor.set_title(std::string("Elite Model Asset Editor ") + elite::model_asset::editor::ModelAssetEditorVersion);
        editor.set_size(1600, 950, WEBVIEW_HINT_NONE);

        server.setOnMessage([&session](const std::string& message) {
            session.handleMessage(message);
        });

        // The editor WebView uses a persistent browser profile. The HTTP server
        // serves the executable-owned resource pack before the filesystem fallback,
        // but the document URL used to be stable across launches. Without an explicit
        // cache policy WebView2 may reuse an older model_asset_editor.html even after
        // the resource pack has been rebuilt. Give every editor process a unique
        // document URL; HtmlUiServer strips the query before resource lookup.
        const auto uiSessionNonce =
            std::chrono::steady_clock::now().time_since_epoch().count();
        const std::string url = "http://localhost:" + std::to_string(port) +
            "/model_asset_editor.html?editor_session=" +
            std::to_string(uiSessionNonce);
        std::cerr << "[MODEL ASSET EDITOR " << elite::model_asset::editor::ModelAssetEditorVersion << "] " << url << std::endl;
        editor.navigate(url);
        editor.run();

        server.stop();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[ModelAssetEditor] fatal: " << ex.what() << '\n';
        return 2;
    }
}
