#pragma once

// The mesh editor's backend (docs/MESH_EDITOR_SPEC.md): the page's commands over the WebSocket, JSON both ways.
// Commands: categories | list {category} | previews {category} | open {category, id} | rescan | refresh {category, id} |
//           settings | saveSettings {sourceRoot, outputRoot, blender} | pick {field, kind: folder|file, current} |
//           inspect {category, id, commit} | setKind {category, id, part, kind: volume|sheet|""} |
//           setCaps {category, id, parts[], on} | blender {category, id, group, part} |
//           assembly {category, id} | saveAssembly {category, id, doc} | build {category, id} | library | saveLibrary {lib} |
//           animCrowd {category} (every skinned model of the category, for the crowd preview) |
//           hubTool (the hub mode: starts tools/models/hub_map_tool.py, answers {type: hubTool, port, error}).
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <optional>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/mesh_editor/MeshEditorCatalog.h"
#include "tools/mesh_editor/MeshEditorHub.h"

class HtmlUiServer;

namespace elite::mesh_editor
{
class Session
{
public:
    Session(Catalog& catalog, HtmlUiServer& server, std::filesystem::path webRoot, std::filesystem::path sourceTree,
            std::filesystem::path settingsFile);
    ~Session();
    void handleMessage(const std::string& text);

private:
    void handleCommand(const std::string& text);

private:
    void send(const nlohmann::json& j);
    // "previews/<cat>/<id>.png?v=<time>"; empty while missing or older than what it shows (then it is re-rendered)
    std::string previewUrl(const ModelCard& c) const;
    // what the preview shows: a module's package glTF once its shell is built (what the game gets), else the source
    std::filesystem::path previewSource(const ModelCard& c) const;
    void enqueuePreviews(const std::string& category);
    void previewWorker();
    nlohmann::json openModel(const ModelCard& c) const;
    using MeshGroup = std::tuple<std::string, std::filesystem::path, std::vector<std::filesystem::path>>;   // name, base, meshes
    static std::vector<MeshGroup> modelGroups(const ModelCard& c);
    nlohmann::json withLinks(nlohmann::json j) const;
    // the module's shell (MESH_EDITOR_SPEC 17): <output>/<category>/<id>/shell/LOD<N>/<link>.obj + a Blender job entry
    // for <id>.blend; the viewer groups "скорлупа LOD<N>" kept in <state>/shell.json
    nlohmann::json buildShell(const ModelCard& card, nlohmann::json& blendJob);
    // the saved shell groups added to an inspection
    nlohmann::json withShell(nlohmann::json j, const ModelCard& c) const;
    // MESH_EDITOR_SPEC 18: the card of a command ("variant": intact or a damaged/<variant>); none when unknown
    std::optional<ModelCard> cardFor(const nlohmann::json& m) const;
    // the model's passport (model.json): read / write; the catalog is rescanned after a write
    nlohmann::json passport(const ModelCard& c) const;
    void savePassport(const ModelCard& c, const nlohmann::json& p);
    // the skinned animation preview of a model (people): its glTF copied into the web cache, the clips, the props
    nlohmann::json animPreview(const ModelCard& c) const;
    nlohmann::json variantsJson(const ModelCard& c, const std::string& select = std::string()) const;
    // Blender, headless: tools/blender/make_module_blend.py over the jobs; true when it ran
    bool runBlendJobs(const nlohmann::json& jobs, const std::string& name);
    void openInBlender(const ModelCard& card, const std::string& group, const std::string& part);
    std::string libraryRef(const std::string& partKey, const std::string& ownCategory, const std::string& ownId) const;
    std::filesystem::path stateDir(const ModelCard& c) const;
    nlohmann::json assemblyJson(const ModelCard& c);
    // a Blender glTF model is read from its converted working copy (MeshEditorGltf); other models as they are
    ModelCard effective(const ModelCard& c);
    // what a fresh assembly document starts from besides the meshes: the glTF scene, the shipped package's calibration
    nlohmann::json seedFor(const ModelCard& c);
    nlohmann::json settingsJson() const;
    void sendCategories();

    Catalog& m_catalog;
    HtmlUiServer& m_server;
    std::filesystem::path m_webRoot;
    std::filesystem::path m_sourceTree;
    std::filesystem::path m_settingsFile;
    std::filesystem::path m_blender;   // the worker's copy (guarded by m_lock)

    std::mutex m_lock;
    std::condition_variable m_wake;
    std::deque<ModelCard> m_queue;
    std::atomic<bool> m_stop {false};
    std::atomic<bool> m_shellBusy {false};
    std::thread m_worker;
    HubTool m_hub;
};
} // namespace elite::mesh_editor
