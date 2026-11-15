#include "tools/mesh_editor/MeshEditorSession.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>

#include "src/ui/html/HtmlUiServer.h"
#include "tools/mesh_editor/MeshEditorInspect.h"
#include "tools/mesh_editor/MeshEditorAssembly.h"
#include "tools/mesh_editor/MeshEditorGltf.h"
#include "tools/mesh_editor/MeshEditorShell.h"
#include "tools/mesh_editor/MeshEditorModule.h"
#include "src/model_asset/ModelAssetBinary.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace elite::mesh_editor
{
namespace fs = std::filesystem;

namespace
{
// the Windows folder / file chooser (runs on its own thread: COM wants a single-threaded apartment); "" = cancelled
std::string pickPath(bool folder, const std::string& currentUtf8)
{
#ifdef _WIN32
    std::string out;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        return out;
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&dlg))))
    {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
        dlg->SetTitle(folder ? L"Выбор папки" : L"Выбор файла");
        std::error_code ec;
        fs::path start = fs::u8path(currentUtf8);
        if (!folder)
            start = start.parent_path();
        while (!start.empty() && !fs::is_directory(start, ec) && start.has_parent_path() && start.parent_path() != start)
            start = start.parent_path();
        IShellItem* at = nullptr;
        if (!start.empty() && fs::is_directory(start, ec) &&
            SUCCEEDED(SHCreateItemFromParsingName(start.make_preferred().wstring().c_str(), nullptr, IID_IShellItem,
                                                  reinterpret_cast<void**>(&at))))
        {
            dlg->SetFolder(at);
            at->Release();
        }
        if (SUCCEEDED(dlg->Show(GetForegroundWindow())))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item)))
            {
                PWSTR name = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)))
                {
                    out = fs::path(name).generic_u8string();
                    CoTaskMemFree(name);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
    CoUninitialize();
    return out;
#else
    (void)folder;
    (void)currentUtf8;
    return std::string();
#endif
}
} // namespace

Session::Session(Catalog& catalog, HtmlUiServer& server, fs::path webRoot, fs::path sourceTree, fs::path settingsFile)
    : m_catalog(catalog), m_server(server), m_webRoot(std::move(webRoot)), m_sourceTree(std::move(sourceTree)),
      m_settingsFile(std::move(settingsFile)), m_blender(catalog.settings().blender)
{
    m_worker = std::thread([this] { previewWorker(); });
}

Session::~Session()
{
    m_stop = true;
    m_wake.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

void Session::send(const nlohmann::json& j)
{
    m_server.broadcastText(j.dump());
}

fs::path Session::previewSource(const ModelCard& c) const
{
    const fs::path gltf = m_catalog.settings().workRoot / c.category / c.id / "blender" / (c.id + ".gltf");
    std::error_code ec;
    return fs::exists(gltf, ec) ? gltf.parent_path() : c.folder;
}

std::string Session::previewUrl(const ModelCard& c) const
{
    const fs::path rel = fs::path("previews") / c.category / (c.id + ".png");
    std::error_code ec;
    const auto shot = fs::last_write_time(m_webRoot / rel, ec);
    if (ec)
        return {};
    // stale when any mesh it shows is newer (the generator ran again, the shell was rebuilt)
    for (fs::recursive_directory_iterator it(previewSource(c), ec), end; !ec && it != end; it.increment(ec))
    {
        const std::string ext = it->path().extension().string();
        if (it->is_regular_file(ec) && (ext == ".obj" || ext == ".mtl" || ext == ".gltf" || ext == ".glb" || ext == ".bin") &&
            fs::last_write_time(it->path(), ec) > shot)
            return {};
    }
    const auto stamp = std::chrono::duration_cast<std::chrono::seconds>(shot.time_since_epoch()).count();
    return rel.generic_u8string() + "?v=" + std::to_string(stamp);
}

void Session::enqueuePreviews(const std::string& category)
{
    std::lock_guard<std::mutex> g(m_lock);
    for (const auto& c : m_catalog.list(category))
    {
        if (!previewUrl(c).empty())
            continue;
        bool queued = false;
        for (const auto& q : m_queue)
            queued = queued || (q.category == c.category && q.id == c.id);
        if (!queued)
            m_queue.push_back(c);
    }
    m_wake.notify_all();
}

// one Blender run per model, in the background; the page asks for the finished ones (command "previews")
void Session::previewWorker()
{
    const fs::path script = m_sourceTree / "tools/mesh_editor/preview_thumbnail.py";
    while (!m_stop)
    {
        // a batch: one Blender start renders up to 20 models (a start costs ~10 s)
        std::vector<ModelCard> batch;
        fs::path blender;
        {
            std::unique_lock<std::mutex> g(m_lock);
            m_wake.wait(g, [&] { return m_stop || !m_queue.empty(); });
            if (m_stop)
                return;
            while (!m_queue.empty() && batch.size() < 20)
            {
                batch.push_back(m_queue.front());
                m_queue.pop_front();
            }
            blender = m_blender;
        }
        std::error_code ec;
        const fs::path list = m_webRoot / "previews" / "batch.txt";
        fs::create_directories(list.parent_path(), ec);
        {
            std::ofstream f(list);
            for (const auto& c : batch)
            {
                const fs::path out = m_webRoot / "previews" / c.category / (c.id + ".png");
                fs::create_directories(out.parent_path(), ec);
                f << previewSource(c).generic_u8string() << '\t' << out.generic_u8string() << '\n';
            }
        }
        const std::string cmd = "\"\"" + blender.string() + "\" -b --factory-startup -P \"" + script.string() +
                                "\" -- --list \"" + list.string() + "\" > NUL 2>&1\"";
        const int rc = std::system(cmd.c_str());
        int ok = 0;
        for (const auto& c : batch)
            ok += fs::exists(m_webRoot / "previews" / c.category / (c.id + ".png"), ec) ? 1 : 0;
        std::cerr << "[MeshEditor] previews: " << ok << " of " << batch.size() << " rendered (rc " << rc << ")" << std::endl;
    }
}

std::vector<Session::MeshGroup> Session::modelGroups(const ModelCard& c)
{
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(c.folder, ec))
        if (e.is_directory() && e.path().filename().string().rfind("LOD", 0) == 0)
            dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    // groups: LOD<N> folders; the mock-up's source/geo/<part>.lod<N>.obj by N; the ship's shells/<name>/ (traffic)
    std::vector<MeshGroup> groups;
    for (const auto& d : dirs)
    {
        std::vector<fs::path> objs;
        for (const auto& f : fs::recursive_directory_iterator(d, ec))
            if (f.is_regular_file() && f.path().extension() == ".obj")
                objs.push_back(f.path());
        groups.push_back({d.filename().string(), d, objs});
    }
    if (dirs.empty())
    {
        std::map<std::string, std::vector<fs::path>> byLod;
        for (const auto& f : fs::directory_iterator(c.folder / "source" / "geo", ec))
        {
            const std::string stem = f.path().stem().string();
            const auto dot = stem.rfind(".lod");
            if (f.path().extension() == ".obj" && dot != std::string::npos)
                byLod["LOD" + stem.substr(dot + 4)].push_back(f.path());
        }
        for (auto& [name, objs] : byLod)
            groups.push_back({name, c.folder / "source" / "geo", objs});
    }
    for (const auto& e : fs::directory_iterator(c.folder / "shells", ec))
        if (e.is_directory())
        {
            std::vector<fs::path> objs;
            for (const auto& f : fs::recursive_directory_iterator(e.path(), ec))
                if (f.is_regular_file() && f.path().extension() == ".obj")
                    objs.push_back(f.path());
            groups.push_back({"скорлупа " + e.path().filename().u8string(), e.path(), objs});
        }
    return groups;
}

nlohmann::json Session::openModel(const ModelCard& c) const
{
    // the first look: what the source holds, LOD by LOD, file by file (the element check of MESH_EDITOR_SPEC 6.1 comes
    // after the author has gone through that window)
    nlohmann::json j = cardJson(c, previewUrl(c));
    j["type"] = "opened";
    nlohmann::json lods = nlohmann::json::array();
    std::error_code ec;
    std::vector<MeshGroup> groups = modelGroups(c);
    for (auto& [groupName, d, objs] : groups)
    {
        nlohmann::json lod;
        lod["name"] = groupName;
        nlohmann::json files = nlohmann::json::array();
        long long vertsTotal = 0, facesTotal = 0;
        std::sort(objs.begin(), objs.end());
        for (const auto& f : objs)
        {
            std::ifstream in(f);
            std::string line;
            long long v = 0, faces = 0;
            while (std::getline(in, line))
            {
                if (line.size() > 2 && line[0] == 'v' && line[1] == ' ')
                    ++v;
                else if (line.size() > 2 && line[0] == 'f' && line[1] == ' ')
                    ++faces;
            }
            vertsTotal += v;
            facesTotal += faces;
            nlohmann::json fj;
            fj["name"] = fs::relative(f, d, ec).generic_u8string();
            fj["vertices"] = v;
            fj["faces"] = faces;
            files.push_back(fj);
        }
        lod["files"] = files;
        lod["vertices"] = vertsTotal;
        lod["faces"] = facesTotal;
        lods.push_back(lod);
    }
    j["lodList"] = lods;
    return j;
}

// parts of an assembly that are library models (a mock-up part named like an equipment folder, later a glTF socket
// with elite_part): shown as links; their own errors are fixed in their card, not counted against the assembly
nlohmann::json Session::withLinks(nlohmann::json j) const
{
    static const char* libraries[] = {"equipment_ship", "equipment_station", "equipment_hub_module"};
    int links = 0, linkErrors = 0;
    for (auto& p : j["parts"])
    {
        const std::string key = p.value("key", std::string());
        for (const char* cat : libraries)
            if (cat != j.value("category", std::string()))
                if (const ModelCard* c = m_catalog.find(cat, key))
                {
                    p["link"] = {{"category", c->category}, {"id", c->id}, {"title", c->title}};
                    ++links;
                    if (p.value("result", std::string()) == "error")
                        ++linkErrors;
                    break;
                }
    }
    j["summary"]["links"] = links;
    j["summary"]["linkErrors"] = linkErrors;
    j["summary"]["error"] = j["summary"].value("error", 0) - linkErrors;
    j["summary"]["ok"] = j["summary"].value("parts", 0) - links - j["summary"].value("error", 0);
    return j;
}

// "Открыть в Blender": a Blender window with the chosen group's meshes (or the model's glTF), the editor stays open
void Session::openInBlender(const ModelCard& card, const std::string& group, const std::string& part)
{
    fs::path blender;
    {
        std::lock_guard<std::mutex> g(m_lock);
        blender = m_blender;
    }
    std::error_code ec;
    if (!fs::is_regular_file(blender, ec))
    {
        send({{"type", "notice"}, {"text", "Blender не найден — укажите его в «Настройках»."}});
        return;
    }
    const fs::path script = m_sourceTree / "tools/mesh_editor/blender_open.py";
    const fs::path list = m_catalog.settings().workRoot / card.category / card.id / "blender_open.json";
    fs::create_directories(list.parent_path(), ec);
    nlohmann::json files = nlohmann::json::array();
    for (const auto& [name, base, groupFiles] : modelGroups(card))
        if (group.empty() || name == group)
            for (const auto& f : groupFiles)
                files.push_back(f.generic_u8string());
    for (const auto& e : fs::directory_iterator(card.folder, ec))
        if (e.path().extension() == ".gltf" || e.path().extension() == ".glb")
            files = nlohmann::json::array({e.path().generic_u8string()});
    std::ofstream(list) << nlohmann::json {{"files", files}, {"select", part}, {"title", card.title}}.dump(1);
    const std::string cmd = "start \"\" \"" + blender.string() + "\" -P \"" + script.string() + "\" -- \"" + list.string() + "\"";
    std::cerr << "[MeshEditor] blender: " << card.category << "/" << card.id << " " << group << " (" << files.size() << " files)" << std::endl;
    std::thread([cmd] { std::system(cmd.c_str()); }).detach();
    send({{"type", "notice"}, {"text", "Blender открывается: " + card.title + (group.empty() ? "" : ", " + group) + "."}});
}

fs::path Session::stateDir(const ModelCard& c) const
{
    const fs::path base = m_catalog.settings().workRoot / c.category / c.id;
    return c.variant == "intact" ? base : base / "variants" / c.variant;
}

namespace
{
// <root>/intact or <root>/damaged/<variant>
fs::path variantDir(const ModelCard& c)
{
    return c.variant == "intact" ? c.root / "intact" : c.root / "damaged" / c.variant;
}

// the newest mesh file of an assembly (what a built shell is compared with), seconds of the file clock (MinGW's file
// clock counts from an epoch where today is negative: the stamp is compared, never shown)
long long assemblyStamp(const fs::path& assembly)
{
    fs::file_time_type t = fs::file_time_type::min();
    std::error_code ec;
    for (fs::recursive_directory_iterator it(assembly, ec), end; !ec && it != end; it.increment(ec))
    {
        const std::string ext = it->path().extension().string();
        const std::string name = it->path().filename().string();
        if (it->is_regular_file(ec) && (ext == ".obj" || ext == ".mtl" || ext == ".gltf" || ext == ".glb" || ext == ".bin" ||
                                        name == "rig.json"))
            t = std::max(t, fs::last_write_time(it->path(), ec));
    }
    if (t == fs::file_time_type::min())
        return std::numeric_limits<long long>::min();
    return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
}

nlohmann::json readJsonFile(const fs::path& p)
{
    std::ifstream in(p);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    return j.is_discarded() ? nlohmann::json() : j;
}
} // namespace

std::optional<ModelCard> Session::cardFor(const nlohmann::json& m) const
{
    const ModelCard* c = m_catalog.find(m.value("category", std::string()), m.value("id", std::string()));
    if (!c)
        return std::nullopt;
    ModelCard v = *c;
    const std::string variant = m.value("variant", std::string("intact"));
    if (variant != "intact")
    {
        const bool known = std::any_of(c->variants.begin(), c->variants.end(), [&](const ModelVariant& x) { return x.id == variant; });
        if (!known)
            return std::nullopt;
        v.variant = variant;
        v.folder = c->root / "damaged" / variant / "assembly";
    }
    return v;
}

nlohmann::json Session::passport(const ModelCard& c) const
{
    nlohmann::json p = readJsonFile(c.root / "model.json");
    return p.is_object() ? p : nlohmann::json {{"schema", "elite.model.v1"}, {"id", c.id}, {"title", c.title}};
}

void Session::savePassport(const ModelCard& c, const nlohmann::json& p)
{
    std::ofstream(c.root / "model.json", std::ios::binary) << p.dump(1) << "\n";
    m_catalog.rescan();
}

nlohmann::json Session::variantsJson(const ModelCard& c, const std::string& select) const
{
    nlohmann::json out = {{"type", "variants"}, {"category", c.category}, {"id", c.id}, {"variants", nlohmann::json::array()}};
    if (const ModelCard* fresh = m_catalog.find(c.category, c.id))
        for (const auto& v : fresh->variants)
            out["variants"].push_back({{"id", v.id}, {"title", v.title}, {"inGame", v.inGame}});
    if (!select.empty())
        out["select"] = select;
    return out;
}

// a part of an assembly named like a library model ("equipment_ship/<id>"), or ""
std::string Session::libraryRef(const std::string& partKey, const std::string& ownCategory) const
{
    static const char* libraries[] = {"equipment_ship", "equipment_station", "equipment_hub_module"};
    for (const char* cat : libraries)
        if (cat != ownCategory)
            if (const ModelCard* c = m_catalog.find(cat, partKey))
                return c->category + "/" + c->id;
    return std::string();
}

ModelCard Session::effective(const ModelCard& c)
{
    if (GltfSource::find(c.folder).empty())
        return c;
    std::string error;
    const fs::path dir = GltfSource::convert(c, stateDir(c), error);
    if (dir.empty())
    {
        std::cerr << "[MeshEditor] glTF " << c.id << ": " << error << std::endl;
        return c;
    }
    // the author's "volume / sheet" from Blender (elite_kind) unless chosen in the editor
    const nlohmann::json scene = GltfSource::scene(stateDir(c));
    std::ifstream in(stateDir(c) / "check.json");
    nlohmann::json state = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json::object();
    in.close();
    const nlohmann::json kinds = scene.value("kinds", nlohmann::json::object());
    for (const auto& [part, kind] : kinds.items())
        if (state.is_discarded() || !state.value("kinds", nlohmann::json::object()).contains(part))
            Inspector::setKind(stateDir(c), part, kind.get<std::string>());
    ModelCard e = c;
    e.folder = dir;
    return e;
}

nlohmann::json Session::seedFor(const ModelCard& c)
{
    nlohmann::json seed = GltfSource::scene(stateDir(c));
    if (!seed.is_object())
        seed = nlohmann::json::object();
    // the shipped package this model replaces (src/assets/compiled/models/<id or singular category>/): its calibration
    std::string singular = c.category;
    if (!singular.empty() && singular.back() == 's')
        singular.pop_back();
    for (const std::string& name : {c.id, singular})
    {
        const fs::path pkg = m_sourceTree / "src/assets/compiled/models" / name / (name + ".elmodel");
        std::error_code ec;
        if (!fs::is_regular_file(pkg, ec))
            continue;
        elite::model_asset::ModelAsset a;
        std::string error;
        bool legacy = false;
        if (!elite::model_asset::ModelAssetBinary::loadManifest(pkg.string(), a, &legacy, &error) || !a.physicalSize.enabled ||
            std::abs(a.physicalSize.sourceToMeters - 1.0f) < 1e-4f)
            continue;
        const int axis = static_cast<int>(a.physicalSize.axis);
        seed["calibration"] = {{"axis", axis == 0 ? "x" : axis == 1 ? "y" : "z"}, {"targetMeters", a.physicalSize.targetMeters},
                               {"sourceExtent", a.physicalSize.sourceExtent},
                               {"from", "calibration of " + fs::relative(pkg, m_sourceTree, ec).generic_u8string()}};
        break;
    }
    return seed;
}

nlohmann::json Session::buildShell(const ModelCard& c0, nlohmann::json& blendJob)
{
    const ModelCard c = effective(c0);
    const fs::path state = stateDir(c);
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    const auto groups = Inspector::repaired(c, state);
    const auto t1 = clock::now();
    const nlohmann::json doc = Assembly::load(c0, groups, [&](const std::string& key) { return libraryRef(key, c.category); }, seedFor(c0));
    const nlohmann::json library = Assembly::loadLibrary(m_sourceTree);
    const RepairedGroup* first = nullptr;
    for (const auto& g : groups)
        if (g.lod && !g.shell)
        {
            first = &g;
            break;
        }
    if (!first)
        return {{"type", "shell"}, {"category", c.category}, {"id", c.id}, {"ok", false}, {"text", "нет LOD для скорлупы"}};
    // the shell lives in the source beside the assembly (MESH_EDITOR_SPEC 18): <variant>/shell/LOD<N>/<id>.gltf +
    // shell.json; a hand-made shell ("source": "manual") is never overwritten
    const fs::path shellDir = variantDir(c0) / "shell";
    const nlohmann::json oldShell = readJsonFile(shellDir / "shell.json");
    if (oldShell.is_object() && oldShell.value("source", std::string()) == "manual")
    {
        send({{"type", "notice"}, {"text", "Скорлупа «" + c.id + "» сделана вручную — редактор её не перестраивает."}});
        return {{"type", "shell"}, {"category", c.category}, {"id", c.id}, {"ok", false}, {"text", "ручная скорлупа"}};
    }
    const auto links = Shell::rig(c.folder, *first);
    const fs::path modelOut = m_catalog.settings().outputRoot / c.category / c.id;
    const fs::path cache = m_webRoot / "cache" / c.category / c.id;
    std::error_code ec;
    fs::remove_all(modelOut / "shell", ec);   // the OBJ shells of the first version (2026-11-14): no longer written
    fs::create_directories(cache, ec);
    ModuleShell module;
    module.id = c.id;
    module.category = c.category;
    module.title = c.title;
    module.links = links;
    module.hitVolumes = doc.value("hit_volumes", nlohmann::json::array());
    module.library = library;
    const std::string stamp = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    nlohmann::json viewGroups = nlohmann::json::array(), stats = nlohmann::json::array();
    std::set<std::string> clearIds;   // library materials marked "transparent": true (glass seen through)
    for (const auto& m : library["materials"])
        if (m.value("transparent", false))
            clearIds.insert(m.value("id", std::string()));
    int n = 0;
    double tVisible = 0;
    const auto t2 = clock::now();
    for (const auto& g : groups)
    {
        if (!g.lod || g.shell)
            continue;
        ShellStats st;
        const auto tv = clock::now();
        const RepairedGroup s = Shell::build(g, links, [&](const std::string& part, const std::string& src) {
            return Assembly::materialId(doc, library, part, src);
        }, [&](const std::string& id) { return clearIds.count(id) > 0; }, st);
        tVisible += std::chrono::duration<double>(clock::now() - tv).count();
        std::vector<float> tris;
        nlohmann::json ranges = nlohmann::json::array();
        for (const auto& p : s.parts)
        {
            if (p.t.empty())
                continue;
            ranges.push_back({{"key", p.key}, {"tri", {tris.size() / 9, p.t.size()}}, {"open", {0, 0}}, {"bad", {0, 0}}, {"sides", 2}});
            for (const auto& t : p.t)
                for (int k = 0; k < 3; ++k)
                    for (int x = 0; x < 3; ++x)
                        tris.push_back(p.v[static_cast<std::size_t>(t[k])][x]);
        }
        const std::string file = "s" + (c.variant == "intact" ? std::string() : "_" + c.variant + "_") + std::to_string(n++) + ".bin";
        std::ofstream(cache / file, std::ios::binary)
            .write(reinterpret_cast<const char*>(tris.data()), static_cast<std::streamsize>(tris.size() * sizeof(float)));
        viewGroups.push_back({{"name", s.name},
                              {"url", (fs::path("cache") / c.category / c.id / file).generic_u8string() + "?v=" + stamp},
                              {"tris", tris.size() / 9}, {"open", 0}, {"bad", 0}, {"parts", ranges}});
        stats.push_back({{"lod", g.name}, {"in", st.trisIn}, {"out", st.trisOut}});
        RepairedGroup named = s;
        named.name = g.name;
        module.lods.push_back(std::move(named));
    }
    // the shell in the source: one glTF per LOD folder, and its stamp
    fs::remove_all(shellDir, ec);
    for (const auto& g : module.lods)
    {
        ModuleShell one = module;
        one.lods = {g};
        ModuleFiles::writeGltf(one, shellDir / g.name / (c.id + ".gltf"), false);
    }
    const long long fromStamp = assemblyStamp(c0.folder);
    std::ofstream(shellDir / "shell.json", std::ios::binary)
        << nlohmann::json {{"schema", "elite.shell.v1"}, {"source", "built"}, {"assemblyStamp", fromStamp}, {"builtAt", stamp},
                           {"lods", stats}, {"links", links.size()}}
               .dump(1)
        << "\n";
    // the module's binary package (game, editor) and the glTF Blender reads for the module's .blend: the intact model
    // (damaged variants join the package as states - MESH_EDITOR_SPEC 18.6 step 3)
    const bool intact = c.variant == "intact";
    const fs::path elmodel = modelOut / (c.id + ".elmodel");
    std::string packageReport = intact ? std::string() : std::string("повреждённый вариант: в пакет войдёт состоянием (позже)");
    const bool packaged = intact ? ModuleFiles::writePackage(module, elmodel, packageReport) : true;
    const fs::path gltf = state / "blender" / (c.id + ".gltf");
    if (intact)
        ModuleFiles::writeGltf(module, gltf);
    nlohmann::json rig = nlohmann::json::array();
    for (const auto& l : links)
        rig.push_back({{"id", l.id}, {"parts", l.parts}, {"parent", l.parent}, {"joint", l.joint}});
    const fs::path blend = modelOut / (c.id + ".blend");
    if (intact)
        blendJob = {{"id", c.id}, {"ref", c.category + "/" + c.id}, {"title", c.title}, {"source", c.folder.generic_u8string()},
                    {"blend", blend.generic_u8string()}, {"gltf", gltf.generic_u8string()}};
    nlohmann::json saved = {{"groups", viewGroups}, {"stats", stats}, {"links", rig.size()}, {"at", stamp},
                            {"blend", intact ? blend.generic_u8string() : std::string()},
                            {"package", intact && packaged ? elmodel.generic_u8string() : std::string()},
                            {"packageReport", packageReport}, {"folder", shellDir.generic_u8string()}};
    fs::create_directories(state, ec);
    std::ofstream(state / "shell.json") << saved.dump(1);
    std::size_t in = 0, kept = 0;
    for (const auto& s : stats)
    {
        in += s["in"].get<std::size_t>();
        kept += s["out"].get<std::size_t>();
    }
    std::cerr << "[MeshEditor] shell " << c.category << "/" << c.id << ": " << kept << " of " << in << " triangles, "
              << links.size() << " links; package " << packageReport << "; s: repair " << std::chrono::duration<double>(t1 - t0).count() << " doc "
              << std::chrono::duration<double>(t2 - t1).count() << " visible " << tVisible << " total "
              << std::chrono::duration<double>(clock::now() - t0).count() << std::endl;
    saved["type"] = "shell";
    saved["category"] = c.category;
    saved["id"] = c.id;
    saved["ok"] = packaged;
    return saved;
}

nlohmann::json Session::withShell(nlohmann::json j, const ModelCard& c) const
{
    const nlohmann::json s = readJsonFile(stateDir(c) / "shell.json");
    // the source shell (MESH_EDITOR_SPEC 18): built from which assembly, or hand-made
    const nlohmann::json src = readJsonFile(variantDir(c) / "shell" / "shell.json");
    std::string status = "none";
    if (src.is_object())
    {
        if (src.value("source", std::string()) == "manual")
            status = "manual";
        else
            status = src.contains("assemblyStamp") && src["assemblyStamp"].get<long long>() >= assemblyStamp(c.folder) ? "built" : "stale";
    }
    if (s.is_object() && s.contains("groups"))
    {
        for (const auto& g : s["groups"])
            j["groups"].push_back(g);
        j["shell"] = {{"stats", s["stats"]}, {"links", s.value("links", 0)}, {"blend", s.value("blend", std::string())},
                      {"package", s.value("package", std::string())}, {"packageReport", s.value("packageReport", std::string())},
                      {"folder", s.value("folder", std::string())}, {"at", s.value("at", std::string())}};
    }
    j["shellStatus"] = status;
    for (const auto& v : c.variants)
        if (v.id == c.variant)
            j["inGame"] = v.inGame;
    return j;
}

bool Session::runBlendJobs(const nlohmann::json& jobs, const std::string& name)
{
    fs::path blender;
    {
        std::lock_guard<std::mutex> g(m_lock);
        blender = m_blender;
    }
    std::error_code ec;
    if (jobs.empty() || !fs::is_regular_file(blender, ec))
        return false;
    const fs::path file = m_catalog.settings().workRoot / ("blend_jobs_" + name + ".json");
    fs::create_directories(file.parent_path(), ec);
    std::ofstream(file) << nlohmann::json {{"jobs", jobs}}.dump(1);
    const fs::path script = m_sourceTree / "tools/blender/make_module_blend.py";
    const fs::path log = m_catalog.settings().workRoot / ("blend_jobs_" + name + ".log");
    // cmd.exe strips the outer quotes of the whole line
    const std::string cmd = "\"\"" + blender.string() + "\" -b --factory-startup -P \"" + script.string() + "\" -- \"" +
                            file.string() + "\" > \"" + log.string() + "\" 2>&1\"";
    const int rc = std::system(cmd.c_str());
    std::cerr << "[MeshEditor] blender jobs " << name << ": " << jobs.size() << " rc=" << rc << " log " << log.generic_u8string() << std::endl;
    return rc == 0;
}

nlohmann::json Session::assemblyJson(const ModelCard& c0)
{
    const ModelCard c = effective(c0);
    const auto groups = Inspector::repaired(c, stateDir(c));
    nlohmann::json doc = Assembly::load(c0, groups, [&](const std::string& key) { return libraryRef(key, c.category); }, seedFor(c0));
    return {{"type", "assembly"}, {"category", c.category}, {"id", c.id}, {"doc", doc}, {"file", Assembly::file(c0).generic_u8string()}};
}

nlohmann::json Session::settingsJson() const
{
    const Settings& s = m_catalog.settings();
    std::error_code ec;
    nlohmann::json j;
    j["type"] = "settings";
    j["sourceRoot"] = s.sourceRoot.generic_u8string();
    j["outputRoot"] = s.outputRoot.generic_u8string();
    j["blender"] = s.blender.generic_u8string();
    j["sourceExists"] = fs::is_directory(s.sourceRoot, ec);
    j["outputExists"] = fs::is_directory(s.outputRoot, ec);
    j["blenderExists"] = fs::is_regular_file(s.blender, ec);
    j["file"] = m_settingsFile.generic_u8string();
    return j;
}

void Session::sendCategories()
{
    nlohmann::json out;
    out["type"] = "categories";
    out["items"] = nlohmann::json::array();
    for (const auto& c : m_catalog.settings().categories)
        out["items"].push_back({{"id", c.id}, {"title", c.title}, {"count", m_catalog.count(c.id)}});
    out["sourceRoot"] = m_catalog.settings().sourceRoot.generic_u8string();
    out["outputRoot"] = m_catalog.settings().outputRoot.generic_u8string();
    send(out);
}

void Session::handleMessage(const std::string& text)
{
    // one bad command must not stop the server: report it to the log and the page
    try
    {
        handleCommand(text);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[MeshEditor] command failed: " << text.substr(0, 120) << ": " << e.what() << std::endl;
        send({{"type", "notice"}, {"text", std::string("Ошибка редактора: ") + e.what()}});
    }
}

void Session::handleCommand(const std::string& text)
{
    const nlohmann::json m = nlohmann::json::parse(text, nullptr, false);
    if (m.is_discarded() || !m.is_object())
        return;
    const std::string cmd = m.value("cmd", std::string());
    if (cmd != "previews" && cmd != "list" && cmd != "categories")
        std::cerr << "[MeshEditor] cmd " << cmd << " " << m.value("category", std::string()) << "/" << m.value("id", std::string()) << std::endl;
    if (cmd == "rescan")
    {
        m_catalog.rescan();
        return;
    }
    if (cmd == "refresh")
    {
        // MESH_EDITOR_SPEC 18.9: the author drops files into the source and presses "Обновить": the catalog is read
        // again (new models with a passport), the open model's damaged/<variant>/ folders become its variants
        m_catalog.rescan();
        sendCategories();
        const ModelCard* c = m_catalog.find(m.value("category", std::string()), m.value("id", std::string()));
        if (!c)
            return;
        nlohmann::json p = passport(*c);
        if (!p.contains("variants") || !p["variants"].is_array())
            p["variants"] = nlohmann::json::array();
        std::vector<std::string> added, noAssembly;
        std::error_code ec;
        for (const auto& v : c->variants)
        {
            if (v.id == "intact")
                continue;
            const bool listed = std::any_of(p["variants"].begin(), p["variants"].end(),
                                            [&](const nlohmann::json& x) { return x.value("id", std::string()) == v.id; });
            if (!listed && fs::is_directory(c->root / "damaged" / v.id, ec))
            {
                p["variants"].push_back({{"id", v.id}, {"title", v.title}, {"inGame", "shell"}});
                added.push_back(v.id);
            }
            if (!fs::is_directory(c->root / "damaged" / v.id / "assembly", ec))
                noAssembly.push_back(v.id);
        }
        if (!added.empty())
        {
            if (!std::any_of(p["variants"].begin(), p["variants"].end(),
                             [](const nlohmann::json& x) { return x.value("id", std::string()) == "intact"; }))
                p["variants"].insert(p["variants"].begin(), {{"id", "intact"}, {"title", "Целая"}, {"inGame", "shell"}});
            savePassport(*c, p);
        }
        std::string text = "Обновлено.";
        if (!added.empty())
        {
            text += " Новые варианты:";
            for (const auto& id : added)
                text += " " + id;
            text += ".";
        }
        for (const auto& id : noAssembly)
            text += " В damaged/" + id + " нет папки assembly — меши кладутся в damaged/" + id + "/assembly/LOD0.";
        nlohmann::json out = variantsJson(*c, std::string());
        out["refresh"] = true;
        send(out);
        send({{"type", "notice"}, {"text", text}});
        return;
    }
    if (cmd == "categories")
    {
        sendCategories();
        return;
    }
    if (cmd == "settings")
    {
        send(settingsJson());
        return;
    }
    if (cmd == "saveSettings")
    {
        Settings s = m_catalog.settings();
        s.sourceRoot = fs::u8path(m.value("sourceRoot", s.sourceRoot.generic_u8string()));
        s.outputRoot = fs::u8path(m.value("outputRoot", s.outputRoot.generic_u8string()));
        s.blender = fs::u8path(m.value("blender", s.blender.generic_u8string()));
        saveSettings(m_settingsFile, s);
        {
            std::lock_guard<std::mutex> g(m_lock);
            m_queue.clear();   // previews of the old folders
            m_blender = s.blender;
        }
        m_catalog.applySettings(s);
        std::cerr << "[MeshEditor] settings saved: source " << s.sourceRoot.generic_u8string() << ", output "
                  << s.outputRoot.generic_u8string() << std::endl;
        send(settingsJson());
        sendCategories();
        return;
    }
    if (cmd == "pick")
    {
        const std::string field = m.value("field", std::string());
        const bool folder = m.value("kind", std::string("folder")) != "file";
        const std::string current = m.value("current", std::string());
        std::thread([this, field, folder, current] {
            const std::string path = pickPath(folder, current);
            send({{"type", "picked"}, {"field", field}, {"path", path}});
        }).detach();
        return;
    }
    const std::string category = m.value("category", std::string());
    if (cmd == "list" || cmd == "previews")
    {
        nlohmann::json out;
        out["type"] = cmd;
        out["category"] = category;
        out["items"] = nlohmann::json::array();
        for (const auto& c : m_catalog.list(category))
            out["items"].push_back(cmd == "list" ? cardJson(c, previewUrl(c))
                                                 : nlohmann::json {{"id", c.id}, {"preview", previewUrl(c)}});
        send(out);
        if (cmd == "list")
            enqueuePreviews(category);
        return;
    }
    if (cmd == "inspect" || cmd == "setKind" || cmd == "setCaps")
    {
        // step 1 "Загрузка и проверка": the repaired geometry for the viewer + the part table; commit = "Проверить"
        if (const auto c = cardFor(m))
        {
            const fs::path state = stateDir(*c);
            if (cmd == "setKind")
                Inspector::setKind(state, m.value("part", std::string()), m.value("kind", std::string()));
            if (cmd == "setCaps")
                for (const auto& part : m.value("parts", std::vector<std::string> {}))
                    Inspector::setCaps(state, part, m.value("on", true));
            nlohmann::json j = withLinks(Inspector::inspect(effective(*c), m_webRoot, state, m.value("commit", false)));
            j["variant"] = c->variant;
            send(withShell(j, *c));
        }
        return;
    }
    if (cmd == "setInGame")
    {
        // MESH_EDITOR_SPEC 18: the passport's variants - what goes into the game
        const auto c = cardFor(m);
        if (!c)
            return;
        nlohmann::json p = passport(*c);
        if (!p.contains("variants") || !p["variants"].is_array())
            p["variants"] = nlohmann::json::array();
        auto entry = [&](const std::string& id) -> nlohmann::json& {
            for (auto& v : p["variants"])
                if (v.value("id", std::string()) == id)
                    return v;
            p["variants"].push_back({{"id", id}, {"title", id == "intact" ? "Целая" : id}, {"inGame", "shell"}});
            return p["variants"].back();
        };
        std::string select;
        const std::string v = m.value("inGame", std::string("shell"));
        entry(c->variant)["inGame"] = v == "assembly+shell" ? v : std::string("shell");
        savePassport(*c, p);
        send(variantsJson(*c, select));
    }
    if (cmd == "shell" || cmd == "shellAll")
    {
        // MESH_EDITOR_SPEC 17: the shells and the module's .blend; the work runs aside, the page gets notices
        if (m_shellBusy.exchange(true))
        {
            send({{"type", "notice"}, {"text", "Скорлупы уже строятся — дождитесь конца."}});
            return;
        }
        std::vector<ModelCard> cards;
        if (cmd == "shell")
        {
            if (const auto c = cardFor(m))
                cards.push_back(*c);
        }
        else
            cards = m_catalog.list(category);
        const bool all = cmd == "shellAll";
        std::thread([this, cards, category, all] {
            nlohmann::json jobs = nlohmann::json::array();
            std::size_t in = 0, kept = 0, done = 0;
            for (const auto& c : cards)
            {
                try
                {
                    nlohmann::json job;
                    nlohmann::json r = buildShell(c, job);
                    if (!job.is_null())
                        jobs.push_back(job);
                    for (const auto& s : r.value("stats", nlohmann::json::array()))
                        if (s["lod"] == "LOD0")
                        {
                            in += s["in"].get<std::size_t>();
                            kept += s["out"].get<std::size_t>();
                        }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[MeshEditor] shell " << c.id << " failed: " << e.what() << std::endl;
                }
                if (all && ++done % 10 == 0)
                    send({{"type", "notice"}, {"text", "Скорлупы: " + std::to_string(done) + " из " + std::to_string(cards.size()) + "…"}});
            }
            // a damaged variant has no .blend of its own (it joins the module's package as a state later)
            const bool ok = jobs.empty() ? true : runBlendJobs(jobs, all ? category : (cards.empty() ? std::string("none") : cards[0].id));
            std::string text = ok ? "Готово: " : "Скорлупы готовы, но Blender не отработал (журнал в рабочей папке): ";
            text += std::to_string(cards.size()) + (cards.size() == 1 ? " модель" : " моделей");
            if (in)
                text += ", LOD0 " + std::to_string(kept) + " из " + std::to_string(in) + " треугольников (" +
                        std::to_string(static_cast<int>(std::lround(100.0 * static_cast<double>(kept) / static_cast<double>(in)))) + "%)";
            send({{"type", "notice"}, {"text", text + "."}});
            enqueuePreviews(category);   // the cards now show the package (stale previews are re-rendered)
            m_shellBusy = false;         // before shellDone: a page may start the next build on it
            for (const auto& c : cards)
                if (!all)
                    send({{"type", "shellDone"}, {"category", c.category}, {"id", c.id}, {"variant", c.variant}, {"blend", ok}});
        }).detach();
        return;
    }
    if (cmd == "library")
    {
        nlohmann::json lib = Assembly::loadLibrary(m_sourceTree);
        send({{"type", "library"}, {"lib", lib}, {"file", Assembly::libraryFile(m_sourceTree).generic_u8string()}});
        return;
    }
    if (cmd == "saveLibrary")
    {
        if (m.contains("lib") && m["lib"].is_object())
            Assembly::saveLibrary(m_sourceTree, m["lib"]);
        send({{"type", "library"}, {"lib", Assembly::loadLibrary(m_sourceTree)}, {"file", Assembly::libraryFile(m_sourceTree).generic_u8string()}});
        return;
    }
    if (cmd == "assembly" || cmd == "saveAssembly" || cmd == "build")
    {
        const auto picked = cardFor(m);
        if (const ModelCard* c = picked ? &*picked : nullptr)
        {
            if (cmd == "saveAssembly" && m.contains("doc") && m["doc"].is_object())
                Assembly::save(*c, m["doc"]);
            if (cmd == "build")
            {
                // the build reads the saved document (or the seed) and the repaired meshes; links come from their cards
                const auto groups = Inspector::repaired(effective(*c), stateDir(*c));
                const nlohmann::json doc =
                    Assembly::load(*c, groups, [&](const std::string& key) { return libraryRef(key, c->category); }, seedFor(*c));
                const nlohmann::json lib = Assembly::loadLibrary(m_sourceTree);
                send(Assembly::build(*c, doc, groups, m_catalog.settings().outputRoot, lib, [&](const std::string& ref) {
                    const auto slash = ref.find('/');
                    const ModelCard* l = slash == std::string::npos ? nullptr : m_catalog.find(ref.substr(0, slash), ref.substr(slash + 1));
                    return l ? Inspector::repaired(effective(*l), stateDir(*l)) : std::vector<RepairedGroup> {};
                }));
                return;
            }
            send(assemblyJson(*c));
        }
        return;
    }
    if (cmd == "blender")
    {
        const auto picked = cardFor(m);
        if (const ModelCard* c = picked ? &*picked : nullptr)
            openInBlender(*c, m.value("group", std::string()), m.value("part", std::string()));
        return;
    }
    if (cmd == "open")
    {
        if (const ModelCard* c = m_catalog.find(category, m.value("id", std::string())))
        {
            std::cerr << "[MeshEditor] open " << category << "/" << c->id << std::endl;
            nlohmann::json j = openModel(effective(*c));
            j["folder"] = c->folder.generic_u8string();
            send(j);
        }
        return;
    }
}
} // namespace elite::mesh_editor
