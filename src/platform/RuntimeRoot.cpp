#include "src/platform/RuntimeRoot.h"

#include <filesystem>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <array>
#include <unistd.h>
#endif

namespace platform
{
namespace
{
namespace fs = std::filesystem;

bool executablePath(fs::path& outPath, std::string* outError)
{
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size())
    );

    if (length == 0 || length >= buffer.size())
    {
        if (outError)
            *outError = "GetModuleFileNameW failed";
        return false;
    }

    buffer.resize(length);
    outPath = fs::path(buffer);
    return true;
#elif defined(__linux__)
    std::array<char, 4096> buffer{};
    const ssize_t length = readlink(
        "/proc/self/exe",
        buffer.data(),
        buffer.size() - 1
    );

    if (length <= 0 || static_cast<std::size_t>(length) >= buffer.size())
    {
        if (outError)
            *outError = "cannot resolve /proc/self/exe";
        return false;
    }

    buffer[static_cast<std::size_t>(length)] = '\0';
    outPath = fs::path(buffer.data());
    return true;
#else
    if (outError)
        *outError = "executable runtime root is unsupported on this platform";
    return false;
#endif
}
}

bool initializeExecutableRuntimeRoot(std::string* outError)
{
    if (outError)
        outError->clear();

    fs::path executable;
    if (!executablePath(executable, outError))
        return false;

    const fs::path runtimeRoot = executable.parent_path();
    if (runtimeRoot.empty())
    {
        if (outError)
            *outError = "resolved executable path has no parent directory";
        return false;
    }

    std::error_code ec;
    fs::current_path(runtimeRoot, ec);
    if (ec)
    {
        if (outError)
            *outError = "cannot switch to executable runtime root: " + ec.message();
        return false;
    }

    return true;
}
}
