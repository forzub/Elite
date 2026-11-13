// Elite mesh editor (docs/MESH_EDITOR_SPEC.md): a new editor on the old core, built step by step with the author.
// EliteMeshEditor                 - the editor window (WebView)
// EliteMeshEditor --serve <s>     - no window: serve the page for <s> seconds (open the printed URL in a browser;
//                                   used for headless checks)
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include <webview/webview.h>

#include "src/ui/html/HtmlUiServer.h"
#include "tools/mesh_editor/MeshEditorCatalog.h"
#include "tools/mesh_editor/MeshEditorSession.h"

#ifndef ELITE_SOURCE_ROOT
#define ELITE_SOURCE_ROOT "."
#endif
#ifndef ELITE_MESH_EDITOR_ROOT
#define ELITE_MESH_EDITOR_ROOT "."
#endif

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    double serveSeconds = -1.0;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--serve")
            serveSeconds = std::atof(argv[i + 1]);
    const fs::path sourceTree = ELITE_SOURCE_ROOT;
    const fs::path root = ELITE_MESH_EDITOR_ROOT;
    // the page: copied from the source tree on every start (edit src/assets/webui/mesh_editor, restart)
    const fs::path web = root / "web";
    std::error_code ec;
    fs::create_directories(web, ec);
    for (const auto& e : fs::directory_iterator(sourceTree / "src/assets/webui/mesh_editor", ec))
        if (e.is_regular_file())
            fs::copy_file(e.path(), web / e.path().filename(), fs::copy_options::overwrite_existing, ec);

    using namespace elite::mesh_editor;
    Catalog catalog(loadSettings(root / "mesh_editor.settings.json", sourceTree), sourceTree);
    HtmlUiServer server;
    const std::uint16_t port = server.start(0, web.string());
    Session session(catalog, server, web, sourceTree);
    server.setOnMessage([&session](const std::string& m) { session.handleMessage(m); });
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string url = "http://localhost:" + std::to_string(port) + "/index.html?s=" + std::to_string(nonce);
    std::cerr << "[MeshEditor] " << url << std::endl;
    if (serveSeconds >= 0.0)
    {
        std::this_thread::sleep_for(std::chrono::duration<double>(serveSeconds));
        server.stop();
        return 0;
    }
    webview::webview window(false, nullptr);
    window.set_title("Elite — мешэдитор");
    window.set_size(1600, 950, WEBVIEW_HINT_NONE);
    window.navigate(url);
    window.run();
    server.stop();
    return 0;
}
