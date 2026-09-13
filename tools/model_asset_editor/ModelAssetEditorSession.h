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
    enum class CatalogSourceAuthority
    {
        RuntimeAssembly,
        Folder
    };

    enum class CatalogBootstrapMode
    {
        Folder,
        RuntimeAssembly
    };

    struct CatalogEntry
    {
        std::string id;
        std::string displayName;
        ObjectType type = ObjectType::None;
        // Optional asset-level folder identity. Folder-authoritative entries
        // import directly from LOD<N>/*.obj. RuntimeAssembly entries may still
        // expose the resolved source folder so the catalog tells the truth about
        // which on-disk model the game currently uses.
        std::filesystem::path sourceDirectory;
        CatalogSourceAuthority sourceAuthority = CatalogSourceAuthority::RuntimeAssembly;
        CatalogBootstrapMode bootstrapMode = CatalogBootstrapMode::RuntimeAssembly;
    };

    void sendCatalog();
    void sendSettings();
    void sendAsset(const std::vector<std::size_t>& payloadLods = {}, bool preserveUiSelection = false);
    void sendAssetMetadata(const nlohmann::json& hints = nlohmann::json::object());
    void sendSemanticTreePatch();
    void sendSurfaceMetadataPatch(const std::vector<std::pair<std::size_t, std::size_t>>& targets);
    void sendSemanticBindingPatch(const std::vector<std::pair<std::size_t, std::size_t>>& targets);
    void sendLodPayload(std::size_t lodIndex, bool includeRawSnapshots = false);
    std::uint32_t nextWireTransferId();
    void sendStatus(const std::string& message, bool error = false, const std::string& activity = "idle", const std::string& messageKey = {}, const nlohmann::json& messageParams = nlohmann::json::object());
    void sendStatusKey(const std::string& messageKey, const nlohmann::json& messageParams, const std::string& fallback, bool error = false, const std::string& activity = "idle");
    void sendProgress(
        const std::string& activity,
        const std::string& stage,
        std::size_t completed,
        std::size_t total,
        const std::filesystem::path& path = {});
    bool selectAsset(const std::string& id, bool forceReimport);
    bool saveAsset(); // ordinary WORKING ASSET save
    bool restoreWorkingAsset(); // discard unsaved edits and reload last saved WORKING ASSET
    bool saveWorkingAsset(bool quiet = false);
    bool buildProductionAsset();
    bool loadLodData(std::size_t lodIndex, bool forceReload, std::string* error = nullptr);
    bool loadLodOnly(std::size_t lodIndex, bool forceReload);
    bool unloadLod(std::size_t lodIndex);
    bool ensureLodLoaded(std::size_t lodIndex);
    bool ensureAllLodsLoaded();
    bool loadAllDeclaredLodsForSource();
    void resetLodState(bool loaded, bool dirty);
    void markManifestDirty();
    void markEditorStateDirty();
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
        const std::filesystem::path& workingFilesRoot,
        const std::string& locale);
    void installLocalizationBundle();
    bool writeSettingsFile();
    bool setLocale(const std::string& locale);

    struct WizardStageState
    {
        std::string status = "not_started"; // not_started / complete / stale / needs_fix
    };
    struct MeshOrientationOverrideRecord
    {
        // "flipped" means invert the entire canonical mesh after automatic PREPARE.
        // The source fingerprint makes this decision stale after a changed OBJ/MTL revision.
        std::string mode;
        std::uint64_t sourceFingerprint = 0;
    };
    struct MeshSourceRecord
    {
        std::string sourceFileName;
        std::string sourcePath;
        std::uint64_t sourceHash = 0;
        // SOURCE inventory found the saved file missing. This is a two-phase
        // deletion marker only: SCAN never removes geometry automatically.
        bool sourceMissing = false;
        // Editor-only SOURCE representation. "geometry" means this SOURCE mesh
        // owns an independent resident RenderGeometryDefinition. "instance"
        // means the SOURCE identity is intentionally represented by one or more
        // RenderNodes that reference another canonical geometry. This survives
        // SAVE/RESTORE so SOURCE reconciliation never resurrects a consolidated
        // duplicate as an independent mesh.
        std::string representation = "geometry";
        std::string instanceOfGeometryId;
        std::vector<std::string> instanceRenderNodeIds;
        // Per-mesh editor-stage evidence. Values are "passed", "failed" or
        // "not_checked". SOURCE replacement/new import resets every stage.
        std::map<std::string, std::string> stageChecks;
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
        std::map<std::size_t, std::map<std::string, MeshOrientationOverrideRecord>> meshOrientationOverrides;
        std::map<std::size_t, std::map<std::string, std::vector<std::string>>> legacySourceVariantReplacements;
        // Ordinary source provenance and granular maintenance debt are editor-only.
        // They never enter the runtime .elmodel contract.
        std::map<std::size_t, std::map<std::string, std::uint64_t>> sourceMeshFingerprints; // source path -> accepted exact file revision
        // Fast metadata stamp used by normal SOURCE CHANGE SCAN. The exact content
        // fingerprint remains authoritative when a source revision is imported/adopted;
        // the quick stamp exists only so a read-only scan never rereads every OBJ.
        std::map<std::size_t, std::map<std::string, std::uint64_t>> sourceMeshQuickStamps;
        std::map<std::size_t, std::map<std::string, MeshSourceRecord>> meshSourceRecords; // geometry id -> provenance + per-stage checks
        std::map<std::string, std::set<std::string>> componentMaintenanceIssues; // base visual id -> prepare/lods/surfaces/semantics
        // Editor-only tree presentation order keyed by stable parent semantic id;
        // "__ROOTS__" stores top-level order. Runtime semantic identity never depends on this.
        std::map<std::string, std::vector<std::string>> semanticChildOrder;
        // Per-render-document SOURCE basis authority. A value such as
        // "blender_model" means that raw SOURCE OBJ geometry for this LOD is
        // converted into game axes when entering WORKING. This is editor-only
        // state because RenderLod payloads must stay independently editable.
        std::map<std::size_t, std::string> lodSourceBasisPresets;
        // Semantic/collision/source hit-volume authoring is shared across LODs;
        // LOD0 owns its one-time SOURCE-frame conversion.
        std::string sharedSourceBasisPreset = "game_current";
        // v2 stores collision/hit primitive orientations in the current LOD0/game
        // frame without accumulating coordinate remaps. v1 is the legacy
        // conjugated-leaf encoding and is migrated on the next explicit LOD0 map.
        int sharedSourceFrameTransformVersion = 2;
        std::size_t nextBaseVisualOrdinal = 1;
        std::size_t nextSourceVariantOrdinal = 1;
    };
    std::filesystem::path wizardWorkspacePath() const;
    std::filesystem::path workingAssetPath() const;
    std::filesystem::path workingEditorStatePath() const;
    std::filesystem::path productionEditorStatePath() const;
    std::filesystem::path wizardLogPath(const std::string& fileName) const;
    std::filesystem::path selectedSourceFilePath(
        const std::string& sourcePath,
        std::string* error = nullptr) const;
    std::filesystem::path selectedSourceAssetRoot() const;
    bool setSourceAssetDirectory(const std::string& rawPath);
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
    nlohmann::json packageStampFor(const std::filesystem::path& manifest) const;
    nlohmann::json productionPackageStamp() const;
    bool productionPackageStampMatches(const nlohmann::json& expected) const;
    bool writeWorkingEditorState(
        const std::string& savedAtUtc,
        std::uint64_t saveRevision,
        std::string* error = nullptr) const;
    bool loadWorkingEditorState(
        EditorAuthoringState& state,
        StageValidityState& validity,
        std::string* savedAtUtc = nullptr,
        std::uint64_t* saveRevision = nullptr,
        std::filesystem::path* sourceAssetDirectory = nullptr,
        std::string* error = nullptr) const;
    bool writeProductionEditorState(std::string* error = nullptr) const;
    bool loadProductionEditorState(
        EditorAuthoringState& state,
        StageValidityState& validity,
        std::uint64_t* saveRevision = nullptr,
        std::filesystem::path* sourceAssetDirectory = nullptr,
        std::string* error = nullptr) const;
    void loadWizardState();
    void invalidateWizardFrom(const std::string& stage);
    bool validateWizardStage(const std::string& stage, std::string* error = nullptr);
    bool validateSurfaceGeometryStage(std::size_t lodIndex, std::size_t geometryIndex, std::string* error = nullptr) const;
    void sendWizardValidationReport();
    bool checkWizardStage(const std::string& stage);
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
        const std::string& scopeGeometryId = {},
        bool onlyUncheckedLodMeshes = false);
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
    void synchronizeMeshSourceRecords(bool preserveChecks = true);
    bool finalizeGeometryInstanceAlias(
        std::size_t lodIndex,
        const std::string& sourceGeometryId,
        const std::string& canonicalGeometryId,
        const std::vector<std::string>& renderNodeIds);
    void resetMeshStageChecks(std::size_t lodIndex, const std::string& geometryId);
    void recordMeshStageResult(const std::string& stage, bool passed, bool markDirty = true);
    void recordSurfaceMeshStageResults(bool markDirty = true);
    bool meshSourceRecordPending(const MeshSourceRecord& record) const;
    nlohmann::json serializeMeshSourceRecords() const;
    nlohmann::json aggregateStageChecksJson() const;
    void sendSourceChangeScan(); // exact-hash scan + targeted SOURCE apply
    bool confirmSourceMeshDeletion(std::size_t lodIndex, const std::string& geometryId);
    bool reloadMeshFromSource(std::size_t lodIndex, std::size_t geometryIndex);
    std::string lodSourceBasisPreset(std::size_t lodIndex) const;
    void applyConfiguredLodBasis(std::size_t lodIndex, MeshLod& mesh) const;
    bool reimportLodSourcePartsInConfiguredBasis(
        std::size_t lodIndex,
        const std::string& requestedPreset = {});
    bool replaceSourcePart(std::size_t lodIndex, std::size_t geometryIndex, bool publish = true, bool rescan = true);
    bool replaceSourcePartByPath(std::size_t lodIndex, const std::string& sourcePath);
    bool addSourcePart(std::size_t lodIndex, const std::string& sourcePath, bool publish = true, bool rescan = true);
    bool importSourceVariantMaintenance(
        std::size_t lodIndex,
        const std::string& sourcePath,
        bool requireExisting,
        bool publish = true,
        bool rescan = true);
    bool prepareOneGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    bool setGeometryOrientationOverride(std::size_t lodIndex, std::size_t geometryIndex, const std::string& mode);
    bool analyzeOneGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    bool regenerateDerivedLodsForGeometry(std::size_t lodIndex, std::size_t geometryIndex);
    std::string maintenanceComponentId(std::size_t lodIndex, const RenderGeometryDefinition& geometry) const;
    void markMaintenanceIssues(const std::string& componentId, std::initializer_list<const char*> issues);
    void clearMaintenanceIssue(const std::string& componentId, const std::string& issue);
    void refreshMaintenanceSemanticIssue(const std::string& componentId);
    nlohmann::json serializeMaintenance() const;

    nlohmann::json serializeAssetMetadata() const;
    nlohmann::json serializeSemanticNodes() const;
    nlohmann::json serializeSemanticTreeOrder() const;

private:
    struct LodEditState
    {
        bool loaded = false;
        bool dirty = false;
    };

    std::filesystem::path m_sourceRoot;
    std::filesystem::path m_sourceAssetsRoot;
    std::filesystem::path m_compiledModelsRoot;
    std::filesystem::path m_workingFilesRoot;
    std::string m_locale = "en";
    HtmlUiServer& m_server;
    std::vector<CatalogEntry> m_catalog;
    ModelAsset m_asset;
    std::string m_selectedId;
    std::string m_openAuthority = "none"; // working / production / source
    std::string m_workingSavedAtUtc;
    std::uint64_t m_workingSaveRevision = 0;
    std::filesystem::path m_loadedSourceAssetDirectory;
    bool m_dirty = false;
    bool m_manifestDirty = false;
    bool m_editorStateDirty = false;
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
    // Persisted editor-only decision relative to automatic canonical orientation.
    // Runtime sees only the already-flipped .elmesh payload.
    std::map<std::size_t, std::map<std::string, MeshOrientationOverrideRecord>> m_meshOrientationOverrides;
    // Session-only RAW snapshots for the diagnostic SOURCE viewport. Never serialized into .elmodel/.elmesh.
    std::map<std::size_t, std::map<std::string, MeshLod>> m_rawMeshSnapshots;
    std::map<std::size_t, std::map<std::string, std::vector<std::string>>> m_legacySourceVariantReplacements;
    std::map<std::size_t, std::map<std::string, std::uint64_t>> m_sourceMeshFingerprints;
    std::map<std::size_t, std::map<std::string, std::uint64_t>> m_sourceMeshQuickStamps;
    std::map<std::size_t, std::map<std::string, MeshSourceRecord>> m_meshSourceRecords;
    std::map<std::string, std::set<std::string>> m_componentMaintenanceIssues;
    std::map<std::string, std::vector<std::string>> m_semanticChildOrder;
    std::map<std::size_t, std::string> m_lodSourceBasisPresets;
    std::string m_sharedSourceBasisPreset = "game_current";
    int m_sharedSourceFrameTransformVersion = 2;
    std::size_t m_nextBaseVisualOrdinal = 1;
    std::size_t m_nextSourceVariantOrdinal = 1;
    std::uint32_t m_nextWireTransferId = 1;
};

} // namespace elite::model_asset::editor
