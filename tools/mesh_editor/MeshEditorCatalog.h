#pragma once

// The mesh editor's model catalog (docs/MESH_EDITOR_SPEC.md 4-5): the source folders by category, one card per model -
// what it is, where it is used, its readiness, its preview. A model folder is <asset>/LOD<N>/*.obj (or a glTF file);
// a category directory that has LOD0 itself is one model too.
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace elite::mesh_editor
{
struct CategoryDef
{
    std::string id;       // equipment_ship, ships, equipment_station, stations, equipment_hub_module, hub_modules, other
    std::string title;    // the button text
    std::vector<std::filesystem::path> sourceDirs;
};

struct ModelCard
{
    std::string category;
    std::string id;
    std::filesystem::path folder;
    std::string title;
    std::string description;
    std::vector<std::string> usedBy;
    std::string status;       // raw / working / ready / stale
    int lods = 0;
    int meshFiles = 0;
    double sizeMb = 0.0;
    bool gltf = false;
};

struct Settings
{
    std::filesystem::path sourceRoot;
    std::filesystem::path outputRoot;
    std::filesystem::path workRoot;
    std::filesystem::path blender;
    std::vector<CategoryDef> categories;
};

// the settings file (created with the defaults when missing)
Settings loadSettings(const std::filesystem::path& file, const std::filesystem::path& sourceTree);

class Catalog
{
public:
    explicit Catalog(Settings settings, const std::filesystem::path& sourceTree);
    const Settings& settings() const { return m_settings; }
    std::vector<ModelCard> list(const std::string& category) const;
    const ModelCard* find(const std::string& category, const std::string& id) const;
    int count(const std::string& category) const;
    void rescan();

private:
    void loadUsage(const std::filesystem::path& sourceTree);
    void describe(ModelCard& card) const;
    Settings m_settings;
    std::vector<ModelCard> m_cards;
    // equipment/<folder> -> product name + the ships that carry it; ship folder -> the designations built on it
    struct Usage
    {
        std::string title;
        std::string description;
        std::vector<std::string> usedBy;
    };
    std::vector<std::pair<std::string, Usage>> m_usage;   // key: "<category>/<folder>"
};

nlohmann::json cardJson(const ModelCard& c, const std::string& previewUrl);
const char* statusText(const std::string& status);
} // namespace elite::mesh_editor
