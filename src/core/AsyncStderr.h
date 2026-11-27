#pragma once

#include "src/core/BuildFlavor.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

namespace core
{
/*
    std::cerr without stalls (2026-11-07): on Windows std::cerr is unbuffered - every `<<` is
    its own WriteFile, and into a console window one trace line of ~60 inserts cost tens of
    milliseconds; a console with a text selection (QuickEdit) blocks every write until the
    selection ends. The author saw periodic freezes while docking and a full hang on the final
    approach (hitch log: "dock_output_handling" 50 ms .. 20.7 s, the docking / dock-traffic /
    HUD logs). std::cerr now appends to a memory buffer; a background thread writes it to the
    real stderr every 50 ms. A buffer over 8 MB drops the excess (counted). Fatal paths call
    drainAsyncStderrNow() before the process dies.
*/
class AsyncStderrBuffer final : public std::streambuf
{
public:
    static constexpr std::size_t CapacityBytes = 8u * 1024u * 1024u;

    AsyncStderrBuffer()
    {
        std::thread([this] { run(); }).detach();
    }

    void drainNow()
    {
        std::string out;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            out.swap(m_pending);
            appendDroppedNote(out);
        }
        write(out);
        if (!out.empty())
        {
            std::lock_guard<std::mutex> lock(m_fileMutex);
            if (m_file)
            {
                std::fwrite(out.data(), 1, out.size(), m_file);
                std::fflush(m_file);
            }
        }
    }

    // SESSION LOG (author 2026-11-23: «в песочнице у тебя все работает, а у меня - все ломается»): every session of
    // the normal game writes its whole log to %LOCALAPPDATA%/EliteGame/logs/<prefix>_<date>_<time>.log (the last 10
    // per prefix are kept) - so a session that broke on the author's machine can be read afterwards.
    // ELITE_SESSION_LOG=0: off.
    void openSessionLog(const char* prefix)
    {
        const char* off = std::getenv("ELITE_SESSION_LOG");
        if (off && off[0] == '0')
            return;
        const char* base = std::getenv("LOCALAPPDATA");
        if (!base || !*base)
            return;
        try
        {
            namespace fs = std::filesystem;
            const fs::path dir = fs::path(base) / core::appDataFolderName() / "logs";
            fs::create_directories(dir);
            // keep the 9 newest of this prefix (+ the new one)
            std::vector<fs::path> old;
            for (const auto& e : fs::directory_iterator(dir))
                if (e.is_regular_file() && e.path().filename().string().rfind(std::string(prefix) + "_", 0) == 0)
                    old.push_back(e.path());
            std::sort(old.begin(), old.end());
            for (std::size_t i = 0; i + 9 < old.size(); ++i)
            {
                std::error_code ec;
                fs::remove(old[i], ec);
            }
            const std::time_t now = std::time(nullptr);
            std::tm tm {};
#ifdef _WIN32
            localtime_s(&tm, &now);
#else
            localtime_r(&now, &tm);
#endif
            char name[96];
            std::snprintf(name, sizeof name, "%s_%04d%02d%02d_%02d%02d%02d.log", prefix, tm.tm_year + 1900, tm.tm_mon + 1,
                          tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            const fs::path path = dir / name;
            std::lock_guard<std::mutex> lock(m_fileMutex);
#ifdef _WIN32
            m_file = _wfopen(path.wstring().c_str(), L"wb");
#else
            m_file = std::fopen(path.string().c_str(), "wb");
#endif
            m_path = path.string();
        }
        catch (const std::exception&)
        {
            m_file = nullptr;
        }
    }
    const std::string& sessionLogPath() const { return m_path; }

protected:
    int_type overflow(int_type c) override
    {
        if (c != traits_type::eof())
        {
            const char ch = static_cast<char>(c);
            xsputn(&ch, 1);
        }
        return traits_type::not_eof(c);
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pending.size() + static_cast<std::size_t>(n) <= CapacityBytes)
            m_pending.append(s, static_cast<std::size_t>(n));
        else
            m_droppedBytes += static_cast<std::size_t>(n);
        return n;
    }

    int sync() override { return 0; }   // std::endl / unitbuf: nothing to wait for

private:
    void appendDroppedNote(std::string& out)
    {
        if (m_droppedBytes == 0)
            return;
        out += "[stderr] " + std::to_string(m_droppedBytes) + " bytes of log dropped (the buffer was full)\n";
        m_droppedBytes = 0;
    }

    static void write(const std::string& out)
    {
        if (out.empty())
            return;
        std::fwrite(out.data(), 1, out.size(), stderr);
        std::fflush(stderr);
    }

    void run()
    {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drainNow();
        }
    }

    std::mutex m_mutex;
    std::string m_pending;
    std::size_t m_droppedBytes = 0;
    std::mutex m_fileMutex;
    std::FILE* m_file = nullptr;
    std::string m_path;
};

inline AsyncStderrBuffer*& asyncStderrInstance()
{
    static AsyncStderrBuffer* instance = nullptr;
    return instance;
}

// std::cerr (and std::cout) -> the async buffer (once; the buffer lives for the whole process), + the session log file
inline void installAsyncStderr(const char* sessionLogPrefix = "client")
{
    if (asyncStderrInstance())
        return;
    asyncStderrInstance() = new AsyncStderrBuffer();
    asyncStderrInstance()->openSessionLog(sessionLogPrefix);
    std::cerr.rdbuf(asyncStderrInstance());
    // stdout joins it: one ordered log (the dev runs read both streams anyway) and the session file gets everything
    std::cout.rdbuf(asyncStderrInstance());
    if (!asyncStderrInstance()->sessionLogPath().empty())
        std::cerr << "[SessionLog] " << asyncStderrInstance()->sessionLogPath() << std::endl;
    std::atexit([] {
        if (auto* b = asyncStderrInstance())
            b->drainNow();
    });
}

// a crash / terminate path: write what is buffered now, on this thread
inline void drainAsyncStderrNow()
{
    if (auto* b = asyncStderrInstance())
        b->drainNow();
}
} // namespace core
