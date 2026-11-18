#include "tools/mesh_editor/MeshEditorHub.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace elite::mesh_editor
{
HubTool::HubTool(fs::path sourceTree, fs::path workRoot) : m_sourceTree(std::move(sourceTree)), m_workRoot(std::move(workRoot)) {}

namespace
{
bool alive(unsigned long pid)
{
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h)
        return false;
    const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return running;
#else
    return pid != 0;
#endif
}
} // namespace

int HubTool::port(std::string& error)
{
    std::lock_guard<std::mutex> g(m_lock);
    if (m_port && alive(m_pid))
        return m_port;
    m_port = 0;
    std::error_code ec;
    fs::create_directories(m_workRoot, ec);
    const fs::path portFile = m_workRoot / "hub_map_tool.port.json";
    const fs::path log = m_workRoot / "hub_map_tool.log";
    fs::remove(portFile, ec);
    const fs::path script = m_sourceTree / "tools/models/hub_map_tool.py";
#ifdef _WIN32
    // python from PATH (the one with the generator's dependencies), no console window, its output to the log
    std::wstring cmd = L"python \"" + script.wstring() + L"\" serve --port-file \"" + portFile.wstring() + L"\" --parent-pid " +
                       std::to_wstring(GetCurrentProcessId());
    SECURITY_ATTRIBUTES sa {sizeof(sa), nullptr, TRUE};
    HANDLE out = CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out;
    si.hStdError = out;
    si.hStdInput = in;
    PROCESS_INFORMATION pi {};
    std::wstring buf = cmd;
    const BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                   m_sourceTree.wstring().c_str(), &si, &pi);
    if (out != INVALID_HANDLE_VALUE)
        CloseHandle(out);
    if (in != INVALID_HANDLE_VALUE)
        CloseHandle(in);
    if (!ok)
    {
        error = "не удалось запустить python (" + std::to_string(GetLastError()) + "): нужен python в PATH";
        return 0;
    }
    m_pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    error = "hub tool: Windows only";
    return 0;
#endif
    for (int i = 0; i < 600; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (fs::exists(portFile, ec))
        {
            const nlohmann::json j = nlohmann::json::parse(std::ifstream(portFile), nullptr, false);
            if (j.is_object() && j.value("port", 0) > 0)
            {
                m_port = j.value("port", 0);
                std::cerr << "[MeshEditor] hub map tool on port " << m_port << " (pid " << m_pid << ")" << std::endl;
                return m_port;
            }
        }
        if (!alive(m_pid))
            break;
    }
    error = "утилита карты хаба не запустилась: " + log.generic_u8string();
    return 0;
}
} // namespace elite::mesh_editor
