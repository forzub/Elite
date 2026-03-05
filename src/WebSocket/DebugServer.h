#pragma once

// Эти определения должны быть ПЕРЕД include
#define _WEBSOCKETPP_CPP11_THREAD_
#define ASIO_STANDALONE 1

// MinGW specific
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <memory>

using WebSocketServer = websocketpp::server<websocketpp::config::asio>;

namespace game::debug
{

class DebugServer
{
public:
    DebugServer();
    ~DebugServer();
    
    void start(int port = 8080);
    void stop();
    void broadcastState(const std::string& jsonData);
    
    // Колбэки для команд
    void onInjectReactorFailure(std::function<void()> callback);
    void onDamageRadiator(std::function<void(int panelIndex)> callback);
    void onRepairAllPanels(std::function<void()> callback);
    void onSetLaserPower(std::function<void(double power)> callback);

private:
    void run();
    void onMessage(websocketpp::connection_hdl hdl, 
                   WebSocketServer::message_ptr msg);
    
    WebSocketServer m_server;
    std::thread m_thread;
    std::atomic<bool> m_running;
    std::map<websocketpp::connection_hdl, 
             std::shared_ptr<void>, 
             std::owner_less<websocketpp::connection_hdl>> m_connections;
    
    // Колбэки
    std::function<void()> m_injectReactorFailure;
    std::function<void(int)> m_damageRadiator;
    std::function<void()> m_repairAllPanels;
    std::function<void(double)> m_setLaserPower;
};

}