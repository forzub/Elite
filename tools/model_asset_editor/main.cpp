#include <filesystem>
#include <iostream>
#include <string>

#include <webview/webview.h>

#include "src/platform/RuntimeRoot.h"
#include "src/ui/html/HtmlUiServer.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "tools/model_asset_editor/ModelAssetEditorSession.h"

#ifndef ELITE_SOURCE_ROOT
#define ELITE_SOURCE_ROOT "."
#endif

int main()
{
    std::string runtimeRootError;
    if (!platform::initializeExecutableRuntimeRoot(&runtimeRootError))
    {
        std::cerr << "[ModelAssetEditor] runtime root error: " << runtimeRootError << '\n';
        return 1;
    }

    try
    {
        ObjectDescriptorRegistry::ensureInitialized();
        game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();

        HtmlUiServer server;
        const std::uint16_t port = server.start(0, "assets/webui");
        elite::model_asset::editor::ModelAssetEditorSession session(
            std::filesystem::path(ELITE_SOURCE_ROOT),
            server
        );

        webview::webview editor(false, nullptr);
        editor.set_title("Elite Model Asset Editor");
        editor.set_size(1600, 950, WEBVIEW_HINT_NONE);

        server.setOnMessage([&session, &editor](const std::string& message) {
            session.handleMessage(message);
            if (session.quitRequested())
                editor.terminate();
        });

        const std::string url = "http://localhost:" + std::to_string(port) +
            "/model_asset_editor.html";
        std::cerr << "[MODEL ASSET EDITOR] " << url << std::endl;
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
