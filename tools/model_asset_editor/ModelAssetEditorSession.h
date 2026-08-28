#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>

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

private:
    struct CatalogEntry
    {
        std::string id;
        std::string displayName;
        ObjectType type = ObjectType::None;
    };

    void sendCatalog();
    void sendSettings();
    void sendAsset();
    void sendAssetMetadata(const nlohmann::json& hints = nlohmann::json::object());
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
    std::filesystem::path settingsPath() const;
    void loadSettings();
    bool saveSettings(
        const std::filesystem::path& sourceAssetsRoot,
        const std::filesystem::path& compiledModelsRoot,
        const std::string& locale);
    void installLocalizationBundle();
    bool writeSettingsFile();
    bool setLocale(const std::string& locale);

    struct WizardStageState
    {
        std::string status = "not_started"; // not_started / complete / stale
        std::filesystem::path checkpointManifest;
    };
    std::filesystem::path wizardWorkspacePath() const;
    std::filesystem::path wizardStatePath() const;
    std::filesystem::path wizardCheckpointPath(const std::string& stage) const;
    std::filesystem::path latestWizardCheckpoint(std::string* stage = nullptr) const;
    void loadWizardState();
    bool writeWizardState() const;
    void invalidateWizardFrom(const std::string& stage);
    void pruneWizardAfter(const std::string& stage);
    bool completeWizardStage(const std::string& stage);
    bool restoreWizardCheckpoint(const std::string& stage);
    bool scanRenderDuplicates(
        std::size_t lodIndex,
        std::size_t referenceRenderNodeIndex = std::size_t(-1),
        const std::vector<std::size_t>& targetRenderNodeIndices = {});
    bool refreshSourceVariants();
    bool setSourceVariantReplacement(
        std::size_t lodIndex,
        const std::string& variantId,
        const std::string& baseVisualId,
        bool allowed);
    std::vector<std::string> sourceVariantReplacementIds(
        const std::string& variantId) const;
    void reconcileAuthoringVisualRegistry();
    std::string sourceVariantAuthoringId(
        std::size_t lodIndex,
        const RenderGeometryDefinition& geometry) const;
    std::string baseVisualId(
        std::size_t lodIndex,
        const std::string& geometryId) const;
    std::string allocateBaseVisualId();
    std::string allocateSourceVariantId();
    nlohmann::json serializeWizard() const;

    nlohmann::json serializeAsset(bool includeGeometryPayload = true) const;

private:
    struct LodEditState
    {
        bool loaded = false;
        bool dirty = false;
    };

    std::filesystem::path m_sourceRoot;
    std::filesystem::path m_sourceAssetsRoot;
    std::filesystem::path m_compiledModelsRoot;
    std::string m_locale = "en";
    HtmlUiServer& m_server;
    std::vector<CatalogEntry> m_catalog;
    ModelAsset m_asset;
    std::string m_selectedId;
    bool m_dirty = false;
    bool m_manifestDirty = false;
    std::vector<LodEditState> m_lodState;
    std::map<std::string, WizardStageState> m_wizardStages;
    // Authoring identities are intentionally independent of OBJ filenames and
    // ephemeral G# indices. A base visual id identifies an intact render family;
    // an extra/variant id identifies an alternate visual. Future generated LODs
    // may reuse those ids without relying on source file names.
    std::map<std::size_t, std::map<std::string, std::string>> m_baseVisualIds; // geometry id -> base visual id
    std::map<std::size_t, std::map<std::string, std::string>> m_sourceExtraMeshIds; // source path -> variant id
    std::map<std::string, std::vector<std::string>> m_sourceVariantReplacements; // variant id -> base visual ids
    std::map<std::size_t, std::map<std::string, std::vector<std::string>>> m_legacySourceVariantReplacements;
    std::size_t m_nextBaseVisualOrdinal = 1;
    std::size_t m_nextSourceVariantOrdinal = 1;
};

} // namespace elite::model_asset::editor
