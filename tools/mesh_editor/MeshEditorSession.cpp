#include "tools/mesh_editor/MeshEditorSession.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "src/ui/html/HtmlUiServer.h"

namespace elite::mesh_editor
{
namespace fs = std::filesystem;

Session::Session(Catalog& catalog, HtmlUiServer& server, fs::path webRoot, fs::path sourceTree)
    : m_catalog(catalog), m_server(server), m_webRoot(std::move(webRoot)), m_sourceTree(std::move(sourceTree))
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
    return fs::exists(m_webRoot / rel, ec) ? rel.generic_string() : std::string();
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
                f << c.folder.string() << '\t' << out.string() << '\n';
            }
        }
        const std::string cmd = "\"\"" + m_catalog.settings().blender.string() + "\" -b --factory-startup -P \"" + script.string() +
                                "\" -- --list \"" + list.string() + "\" > NUL 2>&1\"";
        const int rc = std::system(cmd.c_str());
        int ok = 0;
        for (const auto& c : batch)
            ok += fs::exists(m_webRoot / "previews" / c.category / (c.id + ".png"), ec) ? 1 : 0;
        std::cerr << "[MeshEditor] previews: " << ok << " of " << batch.size() << " rendered (rc " << rc << ")" << std::endl;
    }
}

nlohmann::json Session::openModel(const ModelCard& c) const
{
    // the first look: what the source holds, LOD by LOD, file by file (the element check of MESH_EDITOR_SPEC 6.1 comes
    // after the author has gone through that window)
    nlohmann::json j = cardJson(c, previewUrl(c));
    j["type"] = "opened";
    nlohmann::json lods = nlohmann::json::array();
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(c.folder, ec))
        if (e.is_directory() && e.path().filename().string().rfind("LOD", 0) == 0)
            dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs)
    {
        nlohmann::json lod;
        lod["name"] = d.filename().string();
        nlohmann::json files = nlohmann::json::array();
        long long vertsTotal = 0, facesTotal = 0;
        std::vector<fs::path> objs;
        for (const auto& f : fs::recursive_directory_iterator(d, ec))
            if (f.is_regular_file() && f.path().extension() == ".obj")
                objs.push_back(f.path());
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
            fj["name"] = fs::relative(f, d, ec).generic_string();
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

void Session::handleMessage(const std::string& text)
{
    const nlohmann::json m = nlohmann::json::parse(text, nullptr, false);
    if (m.is_discarded() || !m.is_object())
        return;
    const std::string cmd = m.value("cmd", std::string());
    if (cmd == "rescan")
    {
        m_catalog.rescan();
        return;
    }
    if (cmd == "categories")
    {
        nlohmann::json out;
        out["type"] = "categories";
        out["items"] = nlohmann::json::array();
        for (const auto& c : m_catalog.settings().categories)
            out["items"].push_back({{"id", c.id}, {"title", c.title}, {"count", m_catalog.count(c.id)}});
        out["sourceRoot"] = m_catalog.settings().sourceRoot.generic_string();
        out["outputRoot"] = m_catalog.settings().outputRoot.generic_string();
        send(out);
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
    if (cmd == "open")
    {
        if (const ModelCard* c = m_catalog.find(category, m.value("id", std::string())))
        {
            std::cerr << "[MeshEditor] open " << category << "/" << c->id << std::endl;
            send(openModel(*c));
        }
        return;
    }
}
} // namespace elite::mesh_editor
