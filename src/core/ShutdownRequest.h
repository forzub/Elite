#pragma once

#include <atomic>

namespace core
{
/*
    A graceful quit requested from outside the main loop (the console's Ctrl+C / Ctrl+Break / close, a
    terminating `timeout`). Without it Windows' default handler called ExitProcess on its own thread:
    static destructors and DLL detach (WebView2, the GL driver) ran while the main thread was still
    drawing -> an access violation in opengl32 on every forced shutdown (seen 2026-11-10 .. 11).
    The main loop polls quitRequested() and leaves through its normal shutdown; main() marks
    cleanShutdownDone() at its very end so a console-close handler can wait for it.
*/
inline std::atomic<bool>& quitRequestFlag()
{
    static std::atomic<bool> flag {false};
    return flag;
}

inline std::atomic<bool>& cleanShutdownFlag()
{
    static std::atomic<bool> flag {false};
    return flag;
}

inline void requestQuit() { quitRequestFlag().store(true); }
inline bool quitRequested() { return quitRequestFlag().load(); }
inline void markCleanShutdownDone() { cleanShutdownFlag().store(true); }
inline bool cleanShutdownDone() { return cleanShutdownFlag().load(); }
} // namespace core
