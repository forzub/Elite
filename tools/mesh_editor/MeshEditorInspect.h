#pragma once

// Step 1 "Загрузка и проверка" (docs/MESH_EDITOR_SPEC.md 6.1, agreed with the author 2026-11-13).
// Reads every mesh group of a model (LOD<N>/, the mock-up's source/geo/<part>.lod<N>.obj, shells/<name>/), applies the
// automatic recipe (weld duplicate vertices, drop degenerate and repeated triangles, part pieces that only touch along
// an edge, wind every piece one way, turn closed pieces outward and open ones away from the model centre; on the
// author's "Закрыть дыры" cap simple open loops) and reports what the author fixes in Blender (holes in a volume, edges shared by 3+ faces, no material,
// parts missing between LODs, a LOD that is not lighter). Compares with the last check: changed / new / gone.
// The source files are never written; the viewer gets the repaired triangles as binary files.
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace elite::mesh_editor
{
struct ModelCard;

class Inspector
{
public:
    // webRoot/cache/<category>/<id>/g<N>.bin: per group float32 xyz triangles (all parts), then float32 xyz line
    // segments (the problem edges). stateDir: <workRoot>/<category>/<id> (check.json - the last check, part kinds).
    static nlohmann::json inspect(const ModelCard& card, const std::filesystem::path& webRoot, const std::filesystem::path& stateDir,
                                  bool commitCheck);
    // the author's choice "объём" / "лист" for one part (kept in check.json)
    static void setKind(const std::filesystem::path& stateDir, const std::string& part, const std::string& kind);
    // the author's "Закрыть дыры" for one part (kept in check.json "caps")
    static void setCaps(const std::filesystem::path& stateDir, const std::string& part, bool on);
};
} // namespace elite::mesh_editor
