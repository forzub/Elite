#include "ui/html/HtmlUiServer.h"
#include <filesystem>
#include <iostream>

HtmlUiServer::HtmlUiServer()
{
    m_server.init_asio();
    m_server.set_reuse_addr(true);

    m_server.clear_access_channels(websocketpp::log::alevel::all);
    m_server.clear_error_channels(websocketpp::log::elevel::all);

    m_server.set_open_handler([this](auto hdl) {
        m_connections.insert(hdl);
        std::cout << "[HtmlUiServer] client connected\n";
    });

    m_server.set_close_handler([this](auto hdl) {
        m_connections.erase(hdl);
        std::cout << "[HtmlUiServer] client disconnected\n";
    });

    m_server.set_message_handler([this](auto hdl, auto msg) {
        onMessage(hdl, msg);
    });

    m_server.set_http_handler([this](auto hdl) {
        onHttp(hdl);
    });
}

HtmlUiServer::~HtmlUiServer()
{
    stop();
}

void HtmlUiServer::start(int port, const std::string& rootDir)
{
    m_rootDir = rootDir;

    // WebSocket++ listen(port) uses the configured dual-stack endpoint, so the
    // same HTTP/WebSocket server can be opened from localhost or the LAN.
    m_server.listen(port);
    m_server.start_accept();

    m_running = true;
    m_thread = std::thread([this]() { run(); });

    std::cout << "[HtmlUiServer] started on port " << port << "\n";
    std::cout << "[HtmlUiServer] root dir: " << m_rootDir << "\n";
    std::cout << "[HtmlUiServer] debug UI: http://<this-pc-ip>:"
              << port << "/\n";
}

void HtmlUiServer::stop()
{
    m_running = false;
    try { m_server.stop(); } catch (...) {}

    if (m_thread.joinable())
        m_thread.join();
}

void HtmlUiServer::run()
{
    try
    {
        m_server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HtmlUiServer] run error: " << e.what() << "\n";
    }
}

void HtmlUiServer::setOnMessage(std::function<void(const std::string&)> callback)
{
    m_onMessage = std::move(callback);
}

void HtmlUiServer::setVirtualFile(
    const std::string& resource,
    const std::string& content,
    const std::string& contentType
)
{
    std::string key = resource;
    if (key.empty() || key.front() != '/')
        key.insert(key.begin(), '/');
    m_virtualFiles[key] = VirtualFile {content, contentType};
}





void HtmlUiServer::broadcastText(const std::string& text)
{
    if (!m_running)
        return;

    m_server.get_io_service().post([this, text]()
    {
        std::vector<websocketpp::connection_hdl> dead;

        for (const auto& hdl : m_connections)
        {
            try
            {
                m_server.send(hdl, text, websocketpp::frame::opcode::text);
            }
            catch (...)
            {
                dead.push_back(hdl);
            }
        }

        for (const auto& hdl : dead)
        {
            m_connections.erase(hdl);
        }
    });
}




void HtmlUiServer::onMessage(websocketpp::connection_hdl, WebSocketServer::message_ptr msg)
{
    if (m_onMessage)
        m_onMessage(msg->get_payload());
}

void HtmlUiServer::onHttp(websocketpp::connection_hdl hdl)
{
    auto con = m_server.get_con_from_hdl(hdl);
    std::string resource = con->get_resource();

    const std::size_t suffixPos = resource.find_first_of("?#");
    if (suffixPos != std::string::npos)
        resource.resize(suffixPos);

    if (resource.empty() || resource == "/")
        resource = "/debug_control.html";

    const auto virtualIt = m_virtualFiles.find(resource);
    if (virtualIt != m_virtualFiles.end())
    {
        con->set_status(websocketpp::http::status_code::ok);
        con->replace_header("Content-Type", virtualIt->second.contentType);
        con->set_body(virtualIt->second.content);
        return;
    }

    // This server is intentionally reachable from the local network. Never
    // allow a URL to escape the configured webui root on the host machine.
    if (resource.find('\\') != std::string::npos)
    {
        con->set_status(websocketpp::http::status_code::not_found);
        con->set_body("<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    const std::filesystem::path relativePath =
        std::filesystem::path(resource.substr(1)).lexically_normal();

    if (relativePath.is_absolute())
    {
        con->set_status(websocketpp::http::status_code::not_found);
        con->set_body("<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    for (const auto& part : relativePath)
    {
        if (part == "..")
        {
            con->set_status(websocketpp::http::status_code::not_found);
            con->set_body("<html><body><h1>404 Not Found</h1></body></html>");
            return;
        }
    }

    const std::filesystem::path fullPath =
        std::filesystem::path(m_rootDir) / relativePath;
    std::string content = readFile(fullPath.string());

    if (content.empty())
    {
        con->set_status(websocketpp::http::status_code::not_found);
        con->set_body("<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    con->set_status(websocketpp::http::status_code::ok);
    con->replace_header("Content-Type", getContentType(fullPath.filename().string()));
    con->set_body(content);
}

std::string HtmlUiServer::readFile(const std::string& fullPath)
{
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


static bool endsWith(const std::string& str, const std::string& suffix)
{
    if (str.size() < suffix.size())
        return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string HtmlUiServer::getContentType(const std::string& filename)
{
    if (endsWith(filename, ".html")) return "text/html";
    if (endsWith(filename, ".css"))  return "text/css";
    if (endsWith(filename, ".js"))   return "application/javascript";
    if (endsWith(filename, ".json")) return "application/json";
    if (endsWith(filename, ".png"))  return "image/png";
    if (endsWith(filename, ".jpg"))  return "image/jpeg";
    if (endsWith(filename, ".jpeg")) return "image/jpeg";
    return "text/plain";
}