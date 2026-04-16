#pragma once

#define _WEBSOCKETPP_CPP11_THREAD_
#define ASIO_STANDALONE 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <functional>
#include <memory>
#include <fstream>
#include <sstream>
#include <set>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using WebSocketServer = websocketpp::server<websocketpp::config::asio>;

class HtmlUiServer
{
public:
    HtmlUiServer();
    ~HtmlUiServer();

    void start(int port, const std::string& rootDir);
    void stop();

    void broadcastText(const std::string& text);

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

    std::string m_rootDir;
    std::function<void(const std::string&)> m_onMessage;

    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> m_connections;
};