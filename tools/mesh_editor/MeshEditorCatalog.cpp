#include "tools/mesh_editor/MeshEditorCatalog.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace elite::mesh_editor
{
namespace fs = std::filesystem;

namespace
{
nlohmann::json readJson(const fs::path& p)
{
    std::ifstream in(p);
    if (!in)
        return nlohmann::json();
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    return j.is_discarded() ? nlohmann::json() : j;
}

std::string pathString(const fs::path& p)
{
    return p.generic_string();
}

bool isModelFolder(const fs::path& dir)
{
    std::error_code ec;
    if (fs::is_directory(dir / "LOD0", ec))
        return true;
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        const auto ext = e.path().extension().string();
        if (ext == ".gltf" || ext == ".glb")
            return true;
    }
    return false;
}

// "- `<folder>/` - text" lines of a category README
std::map<std::string, std::string> readmeNotes(const fs::path& dir)
{
    std::map<std::string, std::string> out;
    std::ifstream in(dir / "README.md");
    std::string line;
    while (std::getline(in, line))
    {
        const auto a = line.find("- `");
        if (a == std::string::npos || a > 2)
            continue;
        const auto b = line.find('`', a + 3);
        if (b == std::string::npos)
            continue;
        std::string folder = line.substr(a + 3, b - a - 3);
        while (!folder.empty() && folder.back() == '/')
            folder.pop_back();
        const auto dash = line.find(" - ", b);
        if (dash != std::string::npos && out.find(folder) == out.end())
            out[folder] = line.substr(dash + 3);
    }
    return out;
}

fs::file_time_type newestFile(const fs::path& dir)
{
    fs::file_time_type t {};
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(dir, ec))
        if (e.is_regular_file(ec))
            t = std::max(t, e.last_write_time(ec));
    return t;
}
} // namespace

Settings loadSettings(const fs::path& file, const fs::path& sourceTree)
{
    Settings s;
    s.sourceRoot = sourceTree / "assets/models";
    s.outputRoot = sourceTree / "src/assets/compiled/mesh_editor";
    s.workRoot = sourceTree / "build/tools/mesh_editor/work";
    s.blender = "C:/Program Files/Blender Foundation/Blender 5.0/blender.exe";
    // the author's categories (MESH_EDITOR_SPEC 4); today's folders map onto them
    s.categories = {
        {"equipment_ship", "Оборудование кораблей", {"equipment"}},
        {"ships", "Корабли", {"ships"}},
        {"equipment_station", "Оборудование станций", {"equipment_station"}},
        {"stations", "Станции", {"stations"}},
        {"equipment_hub_module", "Оборудование модулей хабов", {"equipment_hub_module"}},
        {"hub_modules", "Модули хабов", {"hub", "hub_modules"}},
        {"other", "Прочие объекты", {"objects", "drones"}},
    };
    const nlohmann::json j = readJson(file);
    if (j.is_object())
    {
        if (j.contains("sourceRoot"))
            s.sourceRoot = j["sourceRoot"].get<std::string>();
        if (j.contains("outputRoot"))
            s.outputRoot = j["outputRoot"].get<std::string>();
        if (j.contains("workRoot"))
            s.workRoot = j["workRoot"].get<std::string>();
        if (j.contains("blender"))
            s.blender = j["blender"].get<std::string>();
        if (j.contains("categories") && j["categories"].is_array())
        {
            s.categories.clear();
            for (const auto& c : j["categories"])
            {
                CategoryDef d;
                d.id = c.value("id", std::string());
                d.title = c.value("title", d.id);
                for (const auto& p : c.value("sourceDirs", nlohmann::json::array()))
                    d.sourceDirs.push_back(p.get<std::string>());
                s.categories.push_back(d);
            }
        }
    }
    else
    {
        // write the defaults so the author sees and edits them
        nlohmann::json out;
        out["sourceRoot"] = pathString(s.sourceRoot);
        out["outputRoot"] = pathString(s.outputRoot);
        out["workRoot"] = pathString(s.workRoot);
        out["blender"] = pathString(s.blender);
        out["categories"] = nlohmann::json::array();
        for (const auto& c : s.categories)
        {
            nlohmann::json d;
            d["id"] = c.id;
            d["title"] = c.title;
            d["sourceDirs"] = nlohmann::json::array();
            for (const auto& p : c.sourceDirs)
                d["sourceDirs"].push_back(pathString(p));
            out["categories"].push_back(d);
        }
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        std::ofstream(file) << out.dump(2);
    }
    return s;
}

Catalog::Catalog(Settings settings, const fs::path& sourceTree) : m_settings(std::move(settings))
{
    loadUsage(sourceTree);
    rescan();
}

void Catalog::loadUsage(const fs::path& sourceTree)
{
    // equipment: the catalog products by their model_asset, and the ships that install them
    const nlohmann::json cat = readJson(sourceTree / "src/assets/data/ships/ship_catalog_3126.json");
    std::map<std::string, std::string> productFolder;   // product id -> "equipment_ship/<folder>"
    if (cat.is_object())
    {
        for (const auto& p : cat.value("products", nlohmann::json::array()))
        {
            const std::string asset = p.value("model_asset", std::string());
            if (asset.rfind("equipment/", 0) != 0)
                continue;
            const std::string key = "equipment_ship/" + asset.substr(10);
            productFolder[p.value("id", std::string())] = key;
            Usage u;
            u.title = p.value("name", asset);
            std::ostringstream d;
            d << p.value("cat", std::string()) << " · " << p.value("mass_t", 0.0) << " т";
            if (p.contains("official_name"))
                d << " · " << p.value("official_name", std::string());
            u.description = d.str();
            m_usage.push_back({key, u});
        }
        for (const auto& s : cat.value("ships", nlohmann::json::array()))
        {
            std::set<std::string> seen;
            for (const auto& i : s.value("installed", nlohmann::json::array()))
            {
                const auto it = productFolder.find(i.value("product", std::string()));
                if (it == productFolder.end() || !seen.insert(it->second).second)
                    continue;
                for (auto& [key, u] : m_usage)
                    if (key == it->second)
                        u.usedBy.push_back(s.value("designation", s.value("name", std::string())));
            }
        }
    }
    // ships: the designations built on each hull folder
    const nlohmann::json ships = readJson(m_settings.sourceRoot / "ships/ship_models.json");
    if (ships.is_object())
        for (const auto& s : ships.value("ships", nlohmann::json::array()))
        {
            const std::string key = "ships/" + s.value("folder", std::string());
            auto it = std::find_if(m_usage.begin(), m_usage.end(), [&](const auto& x) { return x.first == key; });
            if (it == m_usage.end())
            {
                Usage u;
                u.title = s.value("name", std::string());
                u.description = std::string("корпус · ") + (s.value("source", std::string()) == "authored" ? "сделан автором" : "сгенерирован из примитивов");
                m_usage.push_back({key, u});
                it = m_usage.end() - 1;
            }
            it->second.usedBy.push_back(s.value("designation", std::string()));
        }
}

void Catalog::describe(ModelCard& card) const
{
    for (const auto& [key, u] : m_usage)
        if (key == card.category + "/" + card.id)
        {
            card.title = u.title;
            card.description = u.description;
            card.usedBy = u.usedBy;
        }
    // the folder's own description wins (asset.json: title, description, usedBy)
    const nlohmann::json a = readJson(card.folder / "asset.json");
    if (a.is_object())
    {
        card.title = a.value("title", card.title);
        card.description = a.value("description", card.description);
        if (a.contains("usedBy"))
            for (const auto& u : a["usedBy"])
                card.usedBy.push_back(u.get<std::string>());
    }
    if (card.title.empty())
        card.title = card.id;
}

void Catalog::rescan()
{
    m_cards.clear();
    for (const auto& c : m_settings.categories)
        for (const auto& rel : c.sourceDirs)
        {
            const fs::path dir = rel.is_absolute() ? rel : m_settings.sourceRoot / rel;
            std::error_code ec;
            if (!fs::is_directory(dir, ec))
                continue;
            const auto notes = readmeNotes(dir);
            std::vector<fs::path> models;
            if (isModelFolder(dir))
                models.push_back(dir);
            for (const auto& e : fs::directory_iterator(dir, ec))
                if (e.is_directory() && e.path().filename().string().rfind("LOD", 0) != 0 && isModelFolder(e.path()))
                    models.push_back(e.path());
            for (const auto& m : models)
            {
                ModelCard card;
                card.category = c.id;
                card.id = m.filename().string();
                card.folder = m;
                std::uintmax_t bytes = 0;
                for (const auto& f : fs::recursive_directory_iterator(m, ec))
                {
                    if (f.is_directory() && f.path().filename().string().rfind("LOD", 0) == 0 && f.path().parent_path() == m)
                        ++card.lods;
                    if (!f.is_regular_file())
                        continue;
                    bytes += f.file_size();
                    const auto ext = f.path().extension().string();
                    if (ext == ".obj")
                        ++card.meshFiles;
                    if (ext == ".gltf" || ext == ".glb")
                    {
                        ++card.meshFiles;
                        card.gltf = true;
                    }
                }
                card.sizeMb = static_cast<double>(bytes) / (1024.0 * 1024.0);
                if (const auto n = notes.find(card.id); n != notes.end())
                    card.description = n->second;
                describe(card);
                // readiness: the built package against the newest source file; a work folder = in progress
                const fs::path built = m_settings.outputRoot / c.id / (card.id + ".elmodel");
                if (fs::exists(built, ec))
                    card.status = fs::last_write_time(built, ec) >= newestFile(m) ? "ready" : "stale";
                else if (fs::exists(m_settings.workRoot / c.id / card.id, ec))
                    card.status = "working";
                else
                    card.status = "raw";
                m_cards.push_back(std::move(card));
            }
        }
    std::sort(m_cards.begin(), m_cards.end(), [](const ModelCard& a, const ModelCard& b) {
        return a.category != b.category ? a.category < b.category : a.id < b.id;
    });
    std::cerr << "[MeshEditor] catalog: " << m_cards.size() << " model(s)" << std::endl;
}

std::vector<ModelCard> Catalog::list(const std::string& category) const
{
    std::vector<ModelCard> out;
    for (const auto& c : m_cards)
        if (c.category == category)
            out.push_back(c);
    return out;
}

const ModelCard* Catalog::find(const std::string& category, const std::string& id) const
{
    for (const auto& c : m_cards)
        if (c.category == category && c.id == id)
            return &c;
    return nullptr;
}

int Catalog::count(const std::string& category) const
{
    return static_cast<int>(std::count_if(m_cards.begin(), m_cards.end(), [&](const ModelCard& c) { return c.category == category; }));
}

const char* statusText(const std::string& status)
{
    if (status == "ready")
        return "готова";
    if (status == "stale")
        return "устарела";
    if (status == "working")
        return "в работе";
    return "не обработана";
}

nlohmann::json cardJson(const ModelCard& c, const std::string& previewUrl)
{
    nlohmann::json j;
    j["category"] = c.category;
    j["id"] = c.id;
    j["title"] = c.title;
    j["description"] = c.description;
    j["usedBy"] = c.usedBy;
    j["status"] = c.status;
    j["statusText"] = statusText(c.status);
    j["lods"] = c.lods;
    j["meshFiles"] = c.meshFiles;
    j["sizeMb"] = c.sizeMb;
    j["gltf"] = c.gltf;
    j["folder"] = c.folder.generic_string();
    j["preview"] = previewUrl;
    return j;
}
} // namespace elite::mesh_editor
