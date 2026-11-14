#include "tools/mesh_editor/MeshEditorSession.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <tuple>

#include "src/ui/html/HtmlUiServer.h"
#include "tools/mesh_editor/MeshEditorInspect.h"
#include "tools/mesh_editor/MeshEditorAssembly.h"
#include "tools/mesh_editor/MeshEditorGltf.h"
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

std::string Session::previewUrl(const ModelCard& c) const
{
    const fs::path rel = fs::path("previews") / c.category / (c.id + ".png");
    std::error_code ec;
    return fs::exists(m_webRoot / rel, ec) ? rel.generic_u8string() : std::string();
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
                f << c.folder.generic_u8string() << '\t' << out.generic_u8string() << '\n';
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

fs::path Session::stateDir(const ModelCard& c) const { return m_catalog.settings().workRoot / c.category / c.id; }

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
        if (const ModelCard* c = m_catalog.find(category, m.value("id", std::string())))
        {
            const fs::path stateDir = m_catalog.settings().workRoot / c->category / c->id;
            if (cmd == "setKind")
                Inspector::setKind(stateDir, m.value("part", std::string()), m.value("kind", std::string()));
            if (cmd == "setCaps")
                for (const auto& part : m.value("parts", std::vector<std::string> {}))
                    Inspector::setCaps(stateDir, part, m.value("on", true));
            send(withLinks(Inspector::inspect(effective(*c), m_webRoot, stateDir, m.value("commit", false))));
        }
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
        if (const ModelCard* c = m_catalog.find(category, m.value("id", std::string())))
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
        if (const ModelCard* c = m_catalog.find(category, m.value("id", std::string())))
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
