#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>

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
    }

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
};

inline AsyncStderrBuffer*& asyncStderrInstance()
{
    static AsyncStderrBuffer* instance = nullptr;
    return instance;
}

// std::cerr -> the async buffer (once; the buffer lives for the whole process)
inline void installAsyncStderr()
{
    if (asyncStderrInstance())
        return;
    asyncStderrInstance() = new AsyncStderrBuffer();
    std::cerr.rdbuf(asyncStderrInstance());
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
