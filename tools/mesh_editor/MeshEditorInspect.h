#pragma once

// Step 1 "Загрузка и проверка" (docs/MESH_EDITOR_SPEC.md 6.1, agreed with the author 2026-11-13).
// Reads every mesh group of a model (LOD<N>/, the mock-up's source/geo/<part>.lod<N>.obj, shells/<name>/), applies the
// automatic recipe (weld duplicate vertices, drop degenerate and repeated triangles, part pieces that only touch along
// an edge, wind every piece one way, turn closed pieces outward and open ones away from the model centre; on the
// author's "Закрыть дыры" cap simple open loops) and reports what the author fixes in Blender (holes in a volume, edges shared by 3+ faces, no material,
// parts missing between LODs, a LOD that is not lighter). Compares with the last check: changed / new / gone.
// The source files are never written; the viewer gets the repaired triangles as binary files.
#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace elite::mesh_editor
{
struct ModelCard;

// one part of one group after the recipe, for the build (MESH_EDITOR_SPEC 16.6)
struct RepairedPart
{
    std::string key;
    std::string kind;                      // volume / sheet (the author's choice or the guess)
    int sides = 1;                         // 1: closed, outward; 2: draw both sides
    std::vector<std::array<float, 3>> v;   // welded vertices
    std::vector<std::array<int, 3>> t;     // triangles, wound outward
    std::vector<int> tm;                   // material per triangle (index in mtl, -1 none)
    std::vector<std::string> mtl;          // source material names (usemtl; "eq_hole" for closed holes)
    std::array<float, 3> lo {}, hi {};
    std::vector<std::array<float, 6>> open;   // open edges left (segments)
};
struct RepairedGroup
{
    std::string name;   // LOD0... / "скорлупа <name>"
    bool lod = true;
    bool shell = false;
    std::vector<RepairedPart> parts;
};

class Inspector
{
public:
    // webRoot/cache/<category>/<id>/g<N>.bin: per group float32 xyz triangles (all parts), then float32 xyz line
    // segments (the problem edges). stateDir: <workRoot>/<category>/<id> (check.json - the last check, part kinds).
    static nlohmann::json inspect(const ModelCard& card, const std::filesystem::path& webRoot, const std::filesystem::path& stateDir,
                                  bool commitCheck);
    // the author's choice "объём" / "лист" for one part (kept in check.json)
    static void setKind(const std::filesystem::path& stateDir, const std::string& part, const std::string& kind);
    // every group's parts after the recipe (caps and kinds from check.json): what "Сборка" writes
    static std::vector<RepairedGroup> repaired(const ModelCard& card, const std::filesystem::path& stateDir);
    // the author's "Закрыть дыры" for one part (kept in check.json "caps")
    static void setCaps(const std::filesystem::path& stateDir, const std::string& part, bool on);
};
} // namespace elite::mesh_editor
