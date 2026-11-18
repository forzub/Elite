#pragma once
// The hub mode of the mesh editor (docs/MESH_EDITOR_SPEC.md 19): the page edits a hub MAP (schema elite.hub_map.v1,
// tools/models/units/hub_plane.py) and talks to the map tool tools/models/hub_map_tool.py over local HTTP; this side
// only starts that tool (one python process for the editor's lifetime: the hub generator's modules import once).
#include <filesystem>
#include <mutex>
#include <string>

namespace elite::mesh_editor
{
class HubTool
{
public:
    HubTool(std::filesystem::path sourceTree, std::filesystem::path workRoot);
    // the tool's port, starting it on the first call (waits for its port file, up to ~60 s); 0 + error on failure
    int port(std::string& error);

private:
    std::filesystem::path m_sourceTree;
    std::filesystem::path m_workRoot;
    std::mutex m_lock;
    int m_port = 0;
    unsigned long m_pid = 0;
};
} // namespace elite::mesh_editor
