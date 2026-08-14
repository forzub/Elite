#include "src/platform/ProcessSingleInstanceGuard.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
std::string sanitizeInstanceName(std::string value)
{
    for (char& ch : value)
    {
        const bool ok =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-';
        if (!ok)
            ch = '_';
    }

    if (value.empty())
        value = "process";
    return value;
}
}

namespace platform
{
ProcessSingleInstanceGuard::ProcessSingleInstanceGuard(std::string instanceName)
{
    const std::string safeName = sanitizeInstanceName(std::move(instanceName));

#ifdef _WIN32
    const std::string mutexName = "Global\\" + safeName;
    HANDLE handle = ::CreateMutexA(nullptr, FALSE, mutexName.c_str());
    if (!handle)
    {
        std::ostringstream out;
        out << "CreateMutexA failed with Win32 error " << ::GetLastError();
        m_error = out.str();
        return;
    }

    if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ::CloseHandle(handle);
        m_anotherInstanceRunning = true;
        return;
    }

    m_handle = handle;
    m_ownsInstance = true;
#else
    const char* tmpDir = std::getenv("TMPDIR");
    if (!tmpDir || !*tmpDir)
        tmpDir = "/tmp";

    std::ostringstream path;
    path << tmpDir << "/" << safeName << "_" << static_cast<long long>(::getuid()) << ".lock";
    m_lockPath = path.str();

    m_lockFd = ::open(m_lockPath.c_str(), O_CREAT | O_RDWR, 0600);
    if (m_lockFd < 0)
    {
        m_error = std::string("open lock file failed: ") + std::strerror(errno);
        return;
    }

    if (::flock(m_lockFd, LOCK_EX | LOCK_NB) != 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            m_anotherInstanceRunning = true;
        else
            m_error = std::string("flock failed: ") + std::strerror(errno);

        ::close(m_lockFd);
        m_lockFd = -1;
        return;
    }

    const std::string pid = std::to_string(static_cast<long long>(::getpid())) + "\n";
    (void)::ftruncate(m_lockFd, 0);
    (void)::write(m_lockFd, pid.data(), pid.size());
    m_ownsInstance = true;
#endif
}

ProcessSingleInstanceGuard::~ProcessSingleInstanceGuard()
{
#ifdef _WIN32
    if (m_handle)
        ::CloseHandle(static_cast<HANDLE>(m_handle));
#else
    if (m_lockFd >= 0)
    {
        (void)::flock(m_lockFd, LOCK_UN);
        ::close(m_lockFd);
    }
#endif
}
}
