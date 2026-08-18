#pragma once

#define _WEBSOCKETPP_CPP11_THREAD_
#define ASIO_STANDALONE 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <functional>
#include <memory>
#include <fstream>
#include <sstream>
#include <set>

#include "src/ui/platform/UiResourcePack.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using WebSocketServer = websocketpp::server<websocketpp::config::asio>;

class HtmlUiServer
{
public:
    HtmlUiServer();
    ~HtmlUiServer();

    std::uint16_t start(std::uint16_t port, const std::string& rootDir);
    void stop();

    void broadcastText(const std::string& text);

    void setVirtualFile(
        const std::string& resource,
        const std::string& content,
        const std::string& contentType
    );

    void setOnMessage(std::function<void(const std::string&)> callback);

private:
    void run();
    void onMessage(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg);
    void onHttp(websocketpp::connection_hdl hdl);

    std::string readFile(const std::string& fullPath);
    std::string getContentType(const std::string& filename);

private:
    WebSocketServer m_server;
    std::thread m_thread;
    bool m_running = false;
    std::uint16_t m_localPort = 0;

    struct VirtualFile
    {
        std::string content;
        std::string contentType;
    };

    std::string m_rootDir;
    ui::platform::UiResourcePack m_resourcePack;
    std::unordered_map<std::string, VirtualFile> m_virtualFiles;
    std::function<void(const std::string&)> m_onMessage;

    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> m_connections;
};