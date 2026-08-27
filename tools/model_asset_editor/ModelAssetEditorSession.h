#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/model_asset/ModelAsset.h"
#include "src/ui/html/HtmlUiServer.h"
#include "src/world/types/ObjectType.h"

namespace elite::model_asset::editor
{

class ModelAssetEditorSession
{
public:
    ModelAssetEditorSession(std::filesystem::path sourceRoot, HtmlUiServer& server);

    void handleMessage(const std::string& payload);
    bool quitRequested() const noexcept { return m_quitRequested.load(); }

private:
    struct CatalogEntry
    {
        std::string id;
        std::string displayName;
        ObjectType type = ObjectType::None;
    };

    void sendCatalog();
    void sendAsset();
    void sendStatus(const std::string& message, bool error = false, const std::string& activity = "idle");
    void sendProgress(
        const std::string& activity,
        const std::string& stage,
        std::size_t completed,
        std::size_t total,
        const std::filesystem::path& path = {});
    bool selectAsset(const std::string& id, bool forceReimport);
    bool saveAsset();
    bool saveManifestOnly();
    bool saveLodOnly(std::size_t lodIndex);
    bool loadLodOnly(std::size_t lodIndex, bool forceReload);
    bool unloadLod(std::size_t lodIndex);
    bool ensureLodLoaded(std::size_t lodIndex);
    bool ensureAllLodsLoaded();
    void resetLodState(bool loaded, bool dirty);
    void markManifestDirty();
    void markLodDirty(std::size_t lodIndex);
    void markAllLoadedLodsDirty();
    void syncDirty();
    std::size_t lodCount() const;
    std::filesystem::path compiledPath(const std::string& id) const;
    std::filesystem::path legacyCompiledPath(const std::string& id) const;

    nlohmann::json serializeAsset() const;

private:
    struct LodEditState
    {
        bool loaded = false;
        bool dirty = false;
    };

    std::filesystem::path m_sourceRoot;
    HtmlUiServer& m_server;
    std::vector<CatalogEntry> m_catalog;
    ModelAsset m_asset;
    std::string m_selectedId;
    bool m_dirty = false;
    bool m_manifestDirty = false;
    std::vector<LodEditState> m_lodState;
    std::atomic<bool> m_quitRequested {false};
};

} // namespace elite::model_asset::editor
