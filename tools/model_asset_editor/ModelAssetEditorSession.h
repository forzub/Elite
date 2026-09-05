#pragma once

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>
#include <map>
#include <set>

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
        // Optional asset-level folder authority. Mesh membership is discovered
        // from LOD<N>/*.obj; this is not a per-mesh registration list.
        std::filesystem::path sourceDirectory;
    };

    void sendCatalog();
    void sendSettings();
    void sendAsset(const std::vector<std::size_t>& payloadLods = {});
    void sendAssetMetadata(const nlohmann::json& hints = nlohmann::json::object());
    void sendSurfaceMetadataPatch(const std::vector<std::pair<std::size_t, std::size_t>>& targets);
    void sendSemanticBindingPatch(const std::vector<std::pair<std::size_t, std::size_t>>& targets);
    void sendLodPayload(std::size_t lodIndex, bool includeRawSnapshots = false);
    std::uint32_t nextWireTransferId();
    void sendStatus(const std::string& message, bool error = false, const std::string& activity = "idle");
    void sendProgress(
        const std::string& activity,
        const std::string& stage,
        std::size_t completed,
        std::size_t total,
        const std::filesystem::path& path = {});
    bool selectAsset(const std::string& id, bool forceReimport);
    bool saveAsset(); // ordinary WORKING ASSET save
    bool saveWorkingAsset(bool quiet = false);
    bool buildProductionAsset();
    bool saveManifestOnly();
    bool saveLodOnly(std::size_t lodIndex);
    bool loadLodData(std::size_t lodIndex, bool forceReload, std::string* error = nullptr);
    bool loadLodOnly(std::size_t lodIndex, bool forceReload);
    bool unloadLod(std::size_t lodIndex);
    bool ensureLodLoaded(std::size_t lodIndex);
    bool ensureAllLodsLoaded();
    bool loadAllDeclaredLodsForSource();
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
        std::uint64_t checkpointSequence = 0; // 0 = legacy checkpoint without an explicit save sequence
    };
    struct MeshPreparationRecord
    {
        std::string algorithm;
        std::size_t sourceRenderVertices = 0;
        std::size_t sourceTriangles = 0;
        std::size_t geometricPoints = 0;
        std::size_t outputRenderVertices = 0;
        std::size_t outputTriangles = 0;
        std::size_t removedDegenerateTriangles = 0;
        std::size_t removedDuplicateTriangles = 0;
        std::size_t sourceNonManifoldEdges = 0;
        std::size_t normalIslands = 0;
        std::size_t rebuiltEdges = 0;
        std::size_t splitTopologyVertices = 0;
        std::size_t raycastPatches = 0;
        std::size_t raycastFlippedTriangles = 0;
        std::uint64_t outputFingerprint = 0;
    };
    struct EditorAuthoringState
    {
        std::map<std::size_t, std::map<std::string, std::string>> baseVisualIds;
        std::map<std::size_t, std::map<std::string, std::string>> sourceExtraMeshIds;
        std::map<std::string, std::vector<std::string>> sourceVariantReplacements;
        std::map<std::size_t, std::map<std::string, std::string>> geometryTopologyClasses;
        std::map<std::size_t, std::map<std::string, MeshPreparationRecord>> meshPreparationRecords;
        std::map<std::size_t, std::map<std::string, std::vector<std::string>>> legacySourceVariantReplacements;
        // Ordinary source provenance and granular maintenance debt are editor-only.
        // They never enter the runtime .elmodel contract.
        std::map<std::size_t, std::map<std::string, std::uint64_t>> sourceMeshFingerprints; // source path -> accepted file revision
        std::map<std::string, std::set<std::string>> componentMaintenanceIssues; // base visual id -> prepare/lods/surfaces/semantics
        std::size_t nextBaseVisualOrdinal = 1;
        std::size_t nextSourceVariantOrdinal = 1;
    };
    std::filesystem::path wizardWorkspacePath() const;
    std::filesystem::path wizardStatePath() const;
    std::filesystem::path workingAssetPath() const;
    std::filesystem::path workingEditorStatePath() const;
    std::filesystem::path productionEditorStatePath() const;
    std::filesystem::path wizardCheckpointPath(const std::string& stage) const;
    std::filesystem::path wizardCheckpointEditorStatePath(const std::string& stage) const;
    std::filesystem::path wizardLogPath(const std::string& fileName) const;
    std::uint64_t checkpointSequenceForStage(const std::string& stage) const;
    using StageValidityState = std::map<std::string, std::string>;
    EditorAuthoringState captureEditorAuthoringState() const;
    StageValidityState captureStageValidity() const;
    void applyStageValidity(const StageValidityState& state);
    nlohmann::json serializeStageValidity(const StageValidityState& state) const;
    bool parseStageValidity(const nlohmann::json& state, StageValidityState& parsed, std::string* error = nullptr) const;
    void applyEditorAuthoringState(EditorAuthoringState state);
    nlohmann::json serializeEditorAuthoringState(const EditorAuthoringState& state) const;
    bool parseEditorAuthoringState(
        const nlohmann::json& state,
        int schemaVersion,
        EditorAuthoringState& parsed,
        std::string* error = nullptr) const;
    bool writeCheckpointEditorState(
        const std::string& stage,
        const StageValidityState& validity,
        std::uint64_t checkpointSequence,
        std::string* error = nullptr) const;
    bool loadCheckpointEditorState(
        const std::string& stage,
        EditorAuthoringState& state,
        StageValidityState* validity = nullptr,
        std::uint64_t* checkpointSequence = nullptr,
        std::string* error = nullptr) const;
    nlohmann::json packageStampFor(const std::filesystem::path& manifest) const;
    nlohmann::json productionPackageStamp() const;
    bool productionPackageStampMatches(const nlohmann::json& expected) const;
    bool writeWorkingEditorState(std::string* error = nullptr) const;
    bool loadWorkingEditorState(
        EditorAuthoringState& state,
        StageValidityState& validity,
        std::string* error = nullptr) const;
    bool writeProductionEditorState(std::string* error = nullptr) const;
    bool loadProductionEditorState(
        EditorAuthoringState& state,
        StageValidityState& validity,
        std::string* error = nullptr) const;
    void loadWizardState();
    bool writeWizardState() const;
    void invalidateWizardFrom(const std::string& stage);
    void restoreWizardValidityAt(const std::string& stage);
    bool validateWizardStage(const std::string& stage, std::string* error = nullptr);
    void sendWizardValidationReport();
    bool completeWizardStage(const std::string& stage);
    bool createWizardCheckpoint(const std::string& stage);
    bool restoreWizardCheckpoint(const std::string& stage);
    bool scanRenderDuplicates(
        std::size_t lodIndex,
        std::size_t referenceRenderNodeIndex = std::size_t(-1),
        const std::vector<std::size_t>& targetRenderNodeIndices = {});
    bool analyzeModelPreflight();
    bool canonicalizeLoadedWorkingSet(
        const std::string& invalidationStage = {},
        bool reportStatus = false,
        bool* payloadChangedOut = nullptr,
        std::vector<std::size_t>* changedLodsOut = nullptr,
        std::size_t scopeLod = std::size_t(-1),
        const std::string& scopeGeometryId = {});
    bool verifyLoadedWorkingSetCanonical(std::string* reason = nullptr) const;
    bool modelPreflightAllLoadedReady(std::string* reason = nullptr) const;
    bool setGeometryTopologyClass(
        std::size_t lodIndex,
        std::size_t geometryIndex,
        const std::string& topologyClass,
        bool analyzeAfter = false,
        bool publishAfter = true);
    bool modelPreflightReadyForLod(std::string* reason = nullptr) const;
    bool analyzeLodRequirements(std::size_t lodIndex);
    bool setLodRelativeGeometricError(std::size_t lodIndex, double relativeError);
    bool previewLodComponentCull(std::size_t lodIndex, double thresholdMeters);
    bool previewLodCoplanarCollapse(std::size_t lodIndex);
    bool applyGeneratedLods(
        std::size_t sourceLodIndex,
        const nlohmann::json& levels);
    bool refreshSourceVariants(bool sourceOwned = false, bool broadcastUpdates = true);
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
    void captureCurrentSourceFingerprintBaseline();
    void sendSourceChangeScan();
    bool adoptSourceRevision(std::size_t lodIndex, const std::string& sourcePath);
    bool adoptAllSourceRevisions();
    bool replaceSourcePart(std::size_t lodIndex, std::size_t geometryIndex);
    bool addSourcePart(std::size_t lodIndex, const std::string& sourcePath);
    bool importSourceVariantMaintenance(
        std::size_t lodIndex,
        const std::string& sourcePath,
        bool requireExisting);
    bool prepareOneGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    bool analyzeOneGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    bool regenerateDerivedLodsForGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    std::string maintenanceComponentId(std::size_t lodIndex, const RenderGeometryDefinition& geometry) const;
    void markMaintenanceIssues(const std::string& componentId, std::initializer_list<const char*> issues);
    void clearMaintenanceIssue(const std::string& componentId, const std::string& issue);
    void refreshMaintenanceSemanticIssue(const std::string& componentId);
    nlohmann::json serializeMaintenance() const;

    nlohmann::json serializeAssetMetadata() const;

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
    // Preflight topology intent is authoring metadata. Runtime rendering still
    // uses SurfaceMode; breached and closed volumes both render as front-sided
    // shells, while explicit thin sheets map to ThinTwoSided.
    std::map<std::size_t, std::map<std::string, std::string>> m_geometryTopologyClasses; // geometry id -> explicit class
    // Evidence that a resident working mesh was explicitly prepared through
    // CanonicalMeshBuilder. Load/restore/reimport may legally expose RAW meshes;
    // this sidecar only gates downstream LOD authoring when its fingerprint
    // matches the current resident payload.
    std::map<std::size_t, std::map<std::string, MeshPreparationRecord>> m_meshPreparationRecords;
    // Session-only RAW snapshots for the diagnostic SOURCE viewport. Never serialized into .elmodel/.elmesh.
    std::map<std::size_t, std::map<std::string, MeshLod>> m_rawMeshSnapshots;
    std::map<std::size_t, std::map<std::string, std::vector<std::string>>> m_legacySourceVariantReplacements;
    std::map<std::size_t, std::map<std::string, std::uint64_t>> m_sourceMeshFingerprints;
    std::map<std::string, std::set<std::string>> m_componentMaintenanceIssues;
    std::size_t m_nextBaseVisualOrdinal = 1;
    std::size_t m_nextSourceVariantOrdinal = 1;
    std::uint64_t m_nextCheckpointSequence = 1;
    std::uint32_t m_nextWireTransferId = 1;
};

} // namespace elite::model_asset::editor
