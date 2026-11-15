#include "tools/mesh_editor/MeshEditorCatalog.h"

#include <algorithm>
#include <cstdlib>
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
    return p.generic_u8string();   // UTF-8 both ways: the folders may have Cyrillic names
}

// the mock-up layout of tools/ships/ship_asset_source.py: source/ship_asset.json + source/geo/<part>.lod<N>.obj
bool isMockupFolder(const fs::path& dir)
{
    std::error_code ec;
    return fs::is_regular_file(dir / "source" / "ship_asset.json", ec);
}

// a model = a folder with its passport (author 2026-11-15: "подхватывает то, у чего есть паспорт")
bool isModelFolder(const fs::path& dir)
{
    std::error_code ec;
    return fs::is_regular_file(dir / "model.json", ec);
}

// the passport's variants, plus damaged/<variant>/ folders it does not list yet; "intact" first
std::vector<ModelVariant> variantsOf(const fs::path& root, const nlohmann::json& passport)
{
    std::vector<ModelVariant> out;
    for (const auto& v : passport.value("variants", nlohmann::json::array()))
        out.push_back({v.value("id", std::string()), v.value("title", v.value("id", std::string())), v.value("inGame", std::string("shell"))});
    auto has = [&](const std::string& id) {
        return std::any_of(out.begin(), out.end(), [&](const ModelVariant& v) { return v.id == id; });
    };
    if (!has("intact"))
        out.insert(out.begin(), {"intact", "Целая", "shell"});
    std::error_code ec;
    std::vector<std::string> found;
    for (const auto& e : fs::directory_iterator(root / "damaged", ec))
        if (e.is_directory())
            found.push_back(e.path().filename().u8string());
    std::sort(found.begin(), found.end());
    for (const auto& id : found)
        if (!has(id))
            out.push_back({id, id, "shell"});
    return out;
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
    fs::file_time_type t = fs::file_time_type::min();   // MinGW: file times are negative to the clock epoch
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
    const char* env = std::getenv("ELITE_ASSETS_SOURCE");
    s.sourceRoot = (env && *env ? fs::path(env) : (sourceTree / ".." / "assets_source").lexically_normal()) / "models";
    s.outputRoot = sourceTree / "assets/compiled";
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
            s.sourceRoot = fs::u8path(j["sourceRoot"].get<std::string>());
        if (j.contains("outputRoot"))
            s.outputRoot = fs::u8path(j["outputRoot"].get<std::string>());
        if (j.contains("workRoot"))
            s.workRoot = fs::u8path(j["workRoot"].get<std::string>());
        if (j.contains("blender"))
            s.blender = fs::u8path(j["blender"].get<std::string>());
        if (j.contains("categories") && j["categories"].is_array())
        {
            s.categories.clear();
            for (const auto& c : j["categories"])
            {
                CategoryDef d;
                d.id = c.value("id", std::string());
                d.title = c.value("title", d.id);
                for (const auto& p : c.value("sourceDirs", nlohmann::json::array()))
                    d.sourceDirs.push_back(fs::u8path(p.get<std::string>()));
                s.categories.push_back(d);
            }
        }
    }
    else
        saveSettings(file, s);   // the defaults, so the author sees and edits them
    return s;
}

void saveSettings(const fs::path& file, const Settings& s)
{
    {
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
}

Catalog::Catalog(Settings settings, const fs::path& sourceTree) : m_settings(std::move(settings)), m_sourceTree(sourceTree)
{
    loadUsage(sourceTree);
    rescan();
}

void Catalog::applySettings(Settings settings)
{
    m_settings = std::move(settings);
    m_usage.clear();
    loadUsage(m_sourceTree);
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

// the ships of a model that is not a catalog product inherit from what it belongs to (MESH_EDITOR_SPEC 18.11): a pad
// from its unit (the unit's mounts.json names it), a set "unit + pad" (kind assembly) from the modules it links -
// so the object filter ("Cobrett-I") shows the pads and the sets of its drives and reactors too
void Catalog::inheritUsage()
{
    auto add = [](ModelCard& to, const std::vector<std::string>& from) {
        for (const auto& u : from)
            if (std::find(to.usedBy.begin(), to.usedBy.end(), u) == to.usedBy.end())
                to.usedBy.push_back(u);
    };
    auto card = [&](const std::string& category, const std::string& id) -> ModelCard* {
        for (auto& c : m_cards)
            if (c.category == category && c.id == id)
                return &c;
        return nullptr;
    };
    for (auto& unit : m_cards)
    {
        const nlohmann::json mounts = readJson(unit.folder / "mounts.json");
        if (mounts.is_object())
            for (const auto& m : mounts.value("mounts", nlohmann::json::array()))
                if (ModelCard* pad = card(unit.category, m.value("module", std::string())))
                    if (pad != &unit)
                        add(*pad, unit.usedBy);
    }
    for (auto& set : m_cards)
    {
        if (set.kind != "assembly")
            continue;
        const nlohmann::json doc = readJson(set.folder / "assembly.json");
        if (!doc.is_object())
            continue;
        for (const auto& n : doc.value("nodes", nlohmann::json::array()))
        {
            const std::string ref = n.value("ref", std::string());
            const auto slash = ref.find('/');
            if (slash != std::string::npos)
                if (const ModelCard* m = card(ref.substr(0, slash), ref.substr(slash + 1)))
                    if (m != &set)
                        add(set, std::vector<std::string>(m->usedBy));
        }
    }
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
            for (const auto& e : fs::directory_iterator(dir, ec))
                if (e.is_directory() && isModelFolder(e.path()))
                    models.push_back(e.path());
            for (const auto& m : models)
            {
                ModelCard card;
                card.category = c.id;
                card.root = m;
                card.folder = m / "intact" / "assembly";
                const nlohmann::json passport = readJson(m / "model.json");
                card.id = m.filename().u8string();   // the folder names the model (links, packages)
                card.kind = passport.value("kind", std::string());
                card.variants = variantsOf(m, passport);
                std::uintmax_t bytes = 0;
                for (const auto& f : fs::recursive_directory_iterator(m, ec))
                {
                    if (f.is_directory() && f.path().filename().string().rfind("LOD", 0) == 0 && f.path().parent_path() == card.folder)
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
                if (card.lods == 0 && isMockupFolder(card.folder))
                {
                    std::set<std::string> lods;
                    for (const auto& f : fs::directory_iterator(card.folder / "source" / "geo", ec))
                    {
                        const std::string stem = f.path().stem().string();   // <part>.lod<N>
                        const auto dot = stem.rfind(".lod");
                        if (dot != std::string::npos)
                            lods.insert(stem.substr(dot + 1));
                    }
                    card.lods = static_cast<int>(lods.size());
                }
                card.sizeMb = static_cast<double>(bytes) / (1024.0 * 1024.0);
                if (const auto n = notes.find(card.id); n != notes.end())
                    card.description = n->second;
                describe(card);
                card.title = passport.value("title", card.title);
                if (passport.contains("description"))
                    card.description = passport.value("description", card.description);
                // readiness: the built package against the newest file of the intact model (the package holds the
                // intact model only for now; damaged variants join it as states later); a work folder = in progress
                const fs::path built = m_settings.outputRoot / c.id / card.id / (card.id + ".elmodel");
                if (fs::exists(built, ec))
                    card.status = fs::last_write_time(built, ec) >= newestFile(m / "intact") ? "ready" : "stale";
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
    inheritUsage();
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
    j["folder"] = c.folder.generic_u8string();
    j["root"] = c.root.generic_u8string();
    j["kind"] = c.kind;
    j["variants"] = nlohmann::json::array();
    for (const auto& v : c.variants)
        j["variants"].push_back({{"id", v.id}, {"title", v.title}, {"inGame", v.inGame}});
    j["preview"] = previewUrl;
    return j;
}
} // namespace elite::mesh_editor
