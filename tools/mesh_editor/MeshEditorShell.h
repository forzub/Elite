#pragma once

// The shell of a module (docs/MESH_EDITOR_SPEC.md 17, author 2026-11-14): a module is never opened in the game, so it
// is stored as what is seen from outside - thin two-sided sheets, nothing under the outer surface. Elements that move
// together are merged into one rigid shell per link (rig.json beside hit_volumes.json; no file = one link "body").
// Visibility: the repaired LOD is rendered into a depth buffer from many directions around it; a triangle stays when
// it is the nearest surface in some pixel, or when its centre lies on the nearest surface (triangles smaller than a
// pixel). Every LOD gets its own shell.
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/mesh_editor/MeshEditorInspect.h"

namespace elite::mesh_editor
{
struct ShellLink
{
    std::string id;
    std::vector<std::string> parts;   // element keys
    std::string parent;
    nlohmann::json joint;             // {type: hinge / slide, pivot, axis, limits} (rig.json; used by the poses later)
};

struct ShellStats
{
    std::string group;
    std::size_t trisIn = 0;
    std::size_t trisOut = 0;
};

class Shell
{
public:
    // partKey, source material name -> global library id
    using MaterialOf = std::function<std::string(const std::string& partKey, const std::string& source)>;
    // a library id -> see-through (the library's "transparent": glass does not hide what is behind it)
    using SeeThrough = std::function<bool(const std::string& libraryId)>;

    // <model folder>/rig.json, or one link "body" with every element of lod
    static std::vector<ShellLink> rig(const std::filesystem::path& modelFolder, const RepairedGroup& lod);
    // the shell of one LOD group: one part per link (sheet, two-sided), materials = library ids
    static RepairedGroup build(const RepairedGroup& lod, const std::vector<ShellLink>& links, const MaterialOf& material,
                               const SeeThrough& seeThrough, ShellStats& stats);
    // per part, per triangle: seen from outside. clear (optional, per part, per triangle): see-through triangles stay
    // when seen but hide nothing behind them
    static std::vector<std::vector<char>> visible(const std::vector<const RepairedPart*>& parts,
                                                  const std::vector<std::vector<char>>* clear = nullptr, int directions = 256,
                                                  int resolution = 768);
};
} // namespace elite::mesh_editor
