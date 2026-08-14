#pragma once

#include <string>

namespace platform
{
class ProcessSingleInstanceGuard
{
public:
    explicit ProcessSingleInstanceGuard(std::string instanceName);
    ~ProcessSingleInstanceGuard();

    ProcessSingleInstanceGuard(const ProcessSingleInstanceGuard&) = delete;
    ProcessSingleInstanceGuard& operator=(const ProcessSingleInstanceGuard&) = delete;

    bool ownsInstance() const noexcept { return m_ownsInstance; }
    bool anotherInstanceRunning() const noexcept { return m_anotherInstanceRunning; }
    const std::string& error() const noexcept { return m_error; }

private:
    bool m_ownsInstance = false;
    bool m_anotherInstanceRunning = false;
    std::string m_error;

#ifdef _WIN32
    void* m_handle = nullptr;
#else
    int m_lockFd = -1;
    std::string m_lockPath;
#endif
};
}
