#pragma once

// The mesh editor's backend (docs/MESH_EDITOR_SPEC.md): the page's commands over the WebSocket, JSON both ways.
// Commands: categories | list {category} | previews {category} | open {category, id} | rescan |
//           settings | saveSettings {sourceRoot, outputRoot, blender} | pick {field, kind: folder|file, current}.
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "tools/mesh_editor/MeshEditorCatalog.h"

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
    void send(const nlohmann::json& j);
    std::string previewUrl(const ModelCard& c) const;
    void enqueuePreviews(const std::string& category);
    void previewWorker();
    nlohmann::json openModel(const ModelCard& c) const;
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
    std::thread m_worker;
};
} // namespace elite::mesh_editor
