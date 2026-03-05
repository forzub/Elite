#include "DebugServer.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace game::debug
{

DebugServer::DebugServer()
{
    try {
        m_server.init_asio();
        m_server.set_reuse_addr(true);

        m_server.clear_access_channels(websocketpp::log::alevel::all);  // отключить все логи
        m_server.clear_error_channels(websocketpp::log::elevel::all);   // отключить ошибки (осторожно)
        
        m_server.set_open_handler([this](auto hdl) {
            m_connections[hdl] = std::shared_ptr<void>();
            std::cout << "[DebugServer] Client connected" << std::endl;
        });
        
        m_server.set_close_handler([this](auto hdl) {
            m_connections.erase(hdl);
            std::cout << "[DebugServer] Client disconnected" << std::endl;
        });
        
        m_server.set_message_handler([this](auto hdl, auto msg) {
            onMessage(hdl, msg);
        });
        
        m_server.set_fail_handler([](auto hdl) {
            std::cout << "[DebugServer] Connection failed" << std::endl;
        });
    }
    catch (const std::exception& e) {
        std::cerr << "[DebugServer] Init error: " << e.what() << std::endl;
    }
}

DebugServer::~DebugServer()
{
    stop();
}

void DebugServer::start(int port)
{
    try {
        m_server.listen(port);
        m_server.start_accept();
        
        m_running = true;
        m_thread = std::thread([this]() { run(); });
        
        std::cout << "[DebugServer] Started on port " << port << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[DebugServer] Start error: " << e.what() << std::endl;
    }
}

void DebugServer::stop()
{
    m_running = false;
    try {
        m_server.stop();
    }
    catch (...) {}
    
    if (m_thread.joinable())
        m_thread.join();
}

void DebugServer::run()
{
    try {
        m_server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "[DebugServer] Run error: " << e.what() << std::endl;
    }
}

void DebugServer::broadcastState(const std::string& jsonData)
{
    for (auto& conn : m_connections)
    {
        try {
            m_server.send(conn.first, jsonData, websocketpp::frame::opcode::text);
        }
        catch (...) {
            // Игнорируем ошибки отправки конкретному клиенту
        }
    }
}

void DebugServer::onMessage(websocketpp::connection_hdl hdl, 
                            WebSocketServer::message_ptr msg)
{
    try {
        auto payload = msg->get_payload();
        auto j = json::parse(payload);
        
        std::string cmd = j["command"];
        
        if (cmd == "inject_reactor_failure" && m_injectReactorFailure)
            m_injectReactorFailure();
        
        else if (cmd == "damage_radiator" && m_damageRadiator)
            m_damageRadiator(j["panel_index"]);
        
        else if (cmd == "repair_all_panels" && m_repairAllPanels)
            m_repairAllPanels();
        
        else if (cmd == "set_laser_power" && m_setLaserPower)
            m_setLaserPower(j["power"]);
        
        else if (cmd == "ping") {
            json pong = {{"command", "pong"}};
            m_server.send(hdl, pong.dump(), websocketpp::frame::opcode::text);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[DebugServer] Message error: " << e.what() << std::endl;
    }
}

void DebugServer::onInjectReactorFailure(std::function<void()> callback)
{
    m_injectReactorFailure = callback;
}

void DebugServer::onDamageRadiator(std::function<void(int)> callback)
{
    m_damageRadiator = callback;
}

void DebugServer::onRepairAllPanels(std::function<void()> callback)
{
    m_repairAllPanels = callback;
}

void DebugServer::onSetLaserPower(std::function<void(double)> callback)
{
    m_setLaserPower = callback;
}

}