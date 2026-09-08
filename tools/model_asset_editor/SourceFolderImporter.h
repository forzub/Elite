#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"
#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"

namespace elite::model_asset::editor
{

// Folder-authoritative source contract for modern editor assets:
//
//   <asset>/LOD0/*.obj              -> ordinary/default render meshes
//   <asset>/LOD0/variants/**/*.obj  -> hidden replacement meshes
//   <asset>/LOD1/*.obj              -> independent LOD1 render document
//   ...
//
// OBJ files in the LOD root are discovered from the filesystem. They do not
// need to be registered in ObjectAssemblyRegistry or any gameplay descriptor.
// The variants directory is the only subtree that is scanned recursively for
// replacement geometry.
struct SourceFolderMesh
{
    std::size_t lodIndex = 0;
    std::filesystem::path file;
    std::string sourcePath;
};

struct SourceFolderVariant
{
    std::filesystem::path file;
    std::string sourcePath;
};

// Bounded filesystem inventory used by the first phase of SCAN SOURCE CHANGES.
// It records directory entries plus file metadata in one traversal per source
// directory/tree. The inventory itself never opens OBJ/MTL payload bytes and no
// .elmesh package participates. The session then exact-hashes each discovered
// SOURCE file and imports only new/changed hashes. quickStamp is retained only
// for backward-compatible state parsing/diagnostics; exact sourceHash is the
// schema-13 comparison authority.
struct SourceFolderMetadataEntry
{
    std::size_t lodIndex = 0;
    std::filesystem::path file;
    std::string sourcePath;
    bool variant = false;
    std::uint64_t quickStamp = 0;
};

struct SourceFolderMetadataInventory
{
    std::filesystem::path assetRoot;
    std::vector<SourceFolderMetadataEntry> entries;
    std::size_t directoryEnumerations = 0;
    std::size_t metadataFiles = 0;
};

// Resolve an asset-level source directory against the editor's configured
// source root. The relativeDirectory is an asset directory (for example
// "stations"), never a per-mesh registration list.
std::filesystem::path resolveSourceFolderAssetRoot(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory);

bool sourceFolderAssetAvailable(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory);

// Imports all ordinary OBJ files found directly in every contiguous LOD<N>
// directory into independent v4 RenderLod documents. LOD0 also seeds one
// semantic Node per ordinary mesh so SEMANTICS can merge/reparent/rebind them.
// All transforms are zero because the folder contract uses a shared authored
// coordinate frame: vertex coordinates already assemble the model in place.
bool importSourceFolderAsset(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    ObjectType typeId,
    const std::string& assetId,
    const std::string& displayName,
    ModelAsset& out,
    std::string* error = nullptr,
    std::string* warning = nullptr,
    ImportProgressCallback progress = {});


// Discovers ordinary/default OBJ files from the LOD root only. This is the
// maintenance-side counterpart of the initial folder import: it lets an open
// production asset compare its persisted source provenance with the current
// Blender export folder without rebuilding the whole asset.
std::vector<SourceFolderMesh> discoverSourceFolderOrdinaryMeshes(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    std::vector<std::string>* warnings = nullptr);

// Discovers only <LOD>/variants/**/*.obj for one render LOD. Ordinary OBJ files
// in the LOD root are never returned as variants.
std::vector<SourceFolderVariant> discoverSourceFolderVariants(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    std::size_t lodIndex,
    std::vector<std::string>* warnings = nullptr);

bool scanSourceFolderMetadataInventory(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    SourceFolderMetadataInventory& out,
    std::vector<std::string>* warnings = nullptr);

} // namespace elite::model_asset::editor
