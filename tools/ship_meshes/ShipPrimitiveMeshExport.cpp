// EliteShipMeshExport: editor SOURCE folders of the catalog ships, built from
// the runtime descriptors (ShipDescriptor::primitiveBlocks - the same boxes the
// game draws for a ship without an authored mesh), one OBJ per module:
//
//   <assets_source>/models/ships/<folder>/LOD0/<module>.obj   boxes of that module
//   <assets_source>/models/ships/<folder>/LOD0/primitive.mtl  category materials
//   <assets_source>/models/ships/ship_models.json             folder <-> passport index
// <assets_source> (raw sources, outside the repository - author 2026-11-13): --source <dir>, else the environment
// variable ELITE_ASSETS_SOURCE, else <root>/../assets_source
//
// The Model Asset Editor lists every ships/<folder>/LOD<N>/*.obj and names it
// from the index. Folders are written ONLY for index entries with
// "source": "primitive" (or new passports whose folder does not exist yet);
// an "authored" entry (hand-made mesh, e.g. the existing Cobrett folders) and
// any folder the index does not own (unless every OBJ in it carries this tool's
// header) are never touched. Replacing a primitive
// set by an authored mesh = put the new OBJ files in the folder and set the
// entry's source to "authored".
//
//   EliteShipMeshExport [--root <repo>]          write
//   EliteShipMeshExport [--root <repo>] --check  verify, write nothing (exit 1 on drift)
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/geometry/CatalogShipAssembly.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"

namespace fs = std::filesystem;
using nlohmann::json;

namespace
{
constexpr const char* IndexSchema = "elite.ship_models.v1";

// "bharat.ship.c-120.konveyer" -> "c-120-konveyer"
std::string folderOf(const std::string& passportId)
{
    std::string tail = passportId;
    if (const auto at = passportId.find(".ship."); at != std::string::npos)
        tail = passportId.substr(at + 6);
    for (char& c : tail)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-'))
            c = '-';
    return tail;
}

// Cyrillic position codes ("Б-90-A11A1") -> Latin file names; the OBJ object
// name keeps the exact module id.
std::string transliterate(const std::string& utf8)
{
    static const char* upper[32] = {"A", "B", "V", "G", "D", "E", "Zh", "Z", "I", "Y", "K", "L", "M", "N", "O", "P",
                                    "R", "S", "T", "U", "F", "Kh", "Ts", "Ch", "Sh", "Shch", "", "Y", "", "E", "Yu", "Ya"};
    std::string out;
    for (std::size_t i = 0; i < utf8.size();)
    {
        const auto c0 = static_cast<unsigned char>(utf8[i]);
        if (c0 < 0x80)
        {
            out += static_cast<char>(c0);
            ++i;
            continue;
        }
        if ((c0 & 0xE0) == 0xC0 && i + 1 < utf8.size())
        {
            const unsigned cp = ((c0 & 0x1Fu) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
            if (cp >= 0x410 && cp <= 0x42F)
                out += upper[cp - 0x410];
            else if (cp >= 0x430 && cp <= 0x44F)
            {
                std::string l = upper[cp - 0x430];
                for (char& ch : l)
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                out += l;
            }
            else if (cp == 0x401 || cp == 0x451)
                out += cp == 0x401 ? "Yo" : "yo";
            else
                out += '_';
            i += 2;
            continue;
        }
        out += '_';
        ++i;
        while (i < utf8.size() && (static_cast<unsigned char>(utf8[i]) & 0xC0) == 0x80)
            ++i;
    }
    return out;
}

// module -> file stem "<category>__<module id>"
std::string stemOf(const std::string& category, const std::string& moduleId)
{
    std::string s = category + "__" + transliterate(moduleId);
    for (char& c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'))
            c = '_';
    return s;
}

std::string fmt(double v)
{
    char b[32];
    std::snprintf(b, sizeof b, "%.4f", std::abs(v) < 5e-5 ? 0.0 : v);
    return b;
}

std::string boxesObj(const std::string& moduleId, const std::vector<const ShipPrimitiveBlock*>& blocks)
{
    std::ostringstream o;
    o << "# EliteShipMeshExport - primitive placeholder, module " << moduleId << "\n"
      << "# frame: +X right, +Y up, -Z forward (metres); do not edit, see ship_models.json\n"
      << "mtllib primitive.mtl\n"
      << "o " << moduleId << "\n";
    int base = 1;
    int k = 0;
    for (const auto* b : blocks)
    {
        o << "g " << moduleId << "#" << k++ << "\n";
        const glm::vec3 c = b->centerMeters, h = b->halfSizeMeters;
        for (int i = 0; i < 8; ++i)
            o << "v " << fmt(c.x + ((i & 1) ? h.x : -h.x)) << " " << fmt(c.y + ((i & 2) ? h.y : -h.y)) << " "
              << fmt(c.z + ((i & 4) ? h.z : -h.z)) << "\n";
        o << "usemtl cat_" << b->category << "\n";
        // quads, counter-clockwise seen from outside
        static const int faces[6][4] = {{1, 3, 7, 5}, {0, 4, 6, 2}, {2, 6, 7, 3}, {0, 1, 5, 4}, {4, 5, 7, 6}, {0, 2, 3, 1}};
        for (const auto& f : faces)
            o << "f " << base + f[0] << " " << base + f[1] << " " << base + f[2] << " " << base + f[3] << "\n";
        base += 8;
    }
    return o.str();
}

std::string materialsMtl(const std::set<std::string>& categories)
{
    std::ostringstream o;
    o << "# EliteShipMeshExport - category colours of CatalogShipAssembly (designer palette)\n";
    for (const auto& cat : categories)
    {
        const glm::vec3 c = game::ship::geometry::catalog_assembly_detail::colourOf(cat);
        o << "newmtl cat_" << cat << "\nKd " << fmt(c.r) << " " << fmt(c.g) << " " << fmt(c.b) << "\nKs 0.1000 0.1000 0.1000\nNs 16\n\n";
    }
    return o.str();
}

std::string readText(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int drift = 0;

// a folder this tool wrote: every OBJ of every LOD carries the exporter header
bool writtenByExporter(const fs::path& dir)
{
    std::error_code ec;
    int objs = 0;
    for (const auto& f : fs::recursive_directory_iterator(dir, ec))
        if (f.path().extension() == ".obj")
        {
            ++objs;
            if (readText(f.path()).rfind("# EliteShipMeshExport", 0) != 0)
                return false;
        }
    return objs > 0;
}

void emit(const fs::path& p, const std::string& text, bool check)
{
    std::error_code ec;
    if (fs::exists(p, ec) && readText(p) == text)
        return;
    if (check)
    {
        std::cout << "STALE " << p.generic_string() << "\n";
        ++drift;
        return;
    }
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    f << text;
}
} // namespace

int main(int argc, char** argv)
{
    fs::path root = ".";
    fs::path source;
    bool check = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--check")
            check = true;
        else if (a == "--root" && i + 1 < argc)
            root = argv[++i];
        else if (a == "--source" && i + 1 < argc)
            source = argv[++i];
    }
    if (source.empty())
    {
        const char* env = std::getenv("ELITE_ASSETS_SOURCE");
        source = env && *env ? fs::path(env) : fs::absolute(root).lexically_normal() / ".." / "assets_source";
    }
    const fs::path shipsRoot = source / "models" / "ships";
    const fs::path indexPath = shipsRoot / "ship_models.json";

    json index = json::object();
    if (fs::exists(indexPath))
        index = json::parse(readText(indexPath));
    std::map<std::string, json> previous;   // passport id -> entry
    if (index.contains("ships"))
        for (const auto& e : index["ships"])
            previous[e.value("passportId", std::string())] = e;

    const auto& catalog = game::catalog::ships::ShipDesignCatalog::instance();
    auto& registry = game::ship::descriptors::CatalogShipRegistry::instance();
    json ships = json::array();
    int written = 0, kept = 0;
    std::set<std::string> folders;
    for (const auto& s : catalog.ships())
    {
        json entry = json::object();
        if (const auto it = previous.find(s.id); it != previous.end())
            entry = it->second;
        // a ship flying an existing hand-made descriptor keeps its authored mesh folder
        // (assets/ is untracked: the default must survive a lost index)
        const bool existingRuntime = s.runtime.rfind("existing:", 0) == 0;
        const std::string authoredDefault = s.runtime.rfind("existing:EliteCobraMk1:", 0) == 0 ? "cobretty-mk1-int" : folderOf(s.id);
        const std::string folder = entry.value("folder", existingRuntime ? authoredDefault : folderOf(s.id));
        const std::string source = entry.value("source", existingRuntime ? "authored" : "primitive");
        entry["folder"] = folder;
        entry["passportId"] = s.id;
        entry["designation"] = s.designation;
        entry["name"] = s.name;
        entry["class"] = s.shipClass;
        entry["source"] = source;
        // carrier: bays whose product carries craft (the bay is sized by the carried ship, ship_designer.py)
        json bays = json::array();
        for (const auto& item : s.installed)
            if (const auto* p = catalog.findProduct(item.productId); p && p->number("craft_slots") > 0.0)
                bays.push_back({{"code", item.code},
                                {"product", item.productId},
                                {"slots", static_cast<int>(p->number("craft_slots")) * item.quantity},
                                {"craft", p->text("craft")},
                                {"craftShip", p->text("craft_ship")}});
        if (bays.empty())
            entry.erase("carrier");
        else
            entry["carrier"] = {{"bays", bays}};

        // an authored mesh may serve several passports (Cobrett-I N1 / A3); a generated folder has one owner
        if (!folders.insert(folder).second && source == "primitive")
        {
            std::cout << "FAIL folder " << folder << " claimed twice\n";
            return 2;
        }
        const fs::path dir = shipsRoot / folder;
        std::error_code ec;
        if (source != "primitive")
        {
            entry.erase("parts");
            ++kept;
            ships.push_back(entry);
            continue;
        }
        if (previous.find(s.id) == previous.end() && fs::exists(dir, ec) && !writtenByExporter(dir))
        {
            std::cout << "FAIL " << dir.generic_string() << " exists but the index does not own it - not touched\n";
            return 2;
        }
        const auto* d = registry.find(s.id);
        if (!d)
        {
            std::cout << "FAIL " << s.id << ": " << registry.errorOf(s.id) << "\n";
            return 2;
        }
        std::map<std::string, std::vector<const ShipPrimitiveBlock*>> byModule;
        std::set<std::string> categories;
        for (const auto& b : d->primitiveBlocks)
        {
            byModule[b.moduleId.empty() ? std::string("unbound") : b.moduleId].push_back(&b);
            categories.insert(b.category);
        }
        std::set<fs::path> produced;
        const fs::path lod0 = dir / "LOD0";
        for (const auto& [moduleId, blocks] : byModule)
        {
            const fs::path p = lod0 / (stemOf(blocks.front()->category, moduleId) + ".obj");
            if (!produced.insert(p).second)
            {
                std::cout << "FAIL " << s.id << ": two modules map to " << p.filename().string() << "\n";
                return 2;
            }
            emit(p, boxesObj(moduleId, blocks), check);
        }
        emit(lod0 / "primitive.mtl", materialsMtl(categories), check);
        // stale module files of this generated folder only
        if (fs::is_directory(lod0, ec))
            for (const auto& f : fs::directory_iterator(lod0))
                if (f.path().extension() == ".obj" && !produced.count(f.path()))
                {
                    if (check)
                    {
                        std::cout << "STALE (extra) " << f.path().generic_string() << "\n";
                        ++drift;
                    }
                    else
                        fs::remove(f.path(), ec);
                }
        entry["parts"] = static_cast<int>(byModule.size());
        entry["blocks"] = static_cast<int>(d->primitiveBlocks.size());
        ++written;
        ships.push_back(entry);
    }
    index = {{"schema", IndexSchema},
             {"generated_by", "tools/ship_meshes/ShipPrimitiveMeshExport.cpp (EliteShipMeshExport)"},
             {"note", "source=primitive: regenerated from the runtime descriptors; source=authored: never touched"},
             {"ships", ships}};
    emit(indexPath, index.dump(1) + "\n", check);
    std::cout << written << " primitive ship folders, " << kept << " authored kept, " << ships.size() << " catalog ships\n";
    if (check)
        std::cout << (drift ? "SHIP MESH CHECK: STALE (" + std::to_string(drift) + ")" : std::string("SHIP MESH CHECK: PASS")) << "\n";
    return drift ? 1 : 0;
}
