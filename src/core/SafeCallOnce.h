#pragma once

// call_once that never throws out of its callable (architecture audit 2026-11-06 item 6): under MinGW
// an exception escaping std::call_once leaves the flag locked and every later caller deadlocks (the
// cc-1 start-up hang). The callable runs once; its exception is kept and rethrown to EVERY caller,
// outside the call_once, so a failed initialisation fails loudly instead of hanging.
#include <exception>
#include <mutex>

namespace core
{

template <class F>
void safeCallOnce(std::once_flag& flag, std::exception_ptr& failure, F&& f)
{
    std::call_once(flag, [&]() {
        try
        {
            f();
        }
        catch (...)
        {
            failure = std::current_exception();
        }
    });
    if (failure)
        std::rethrow_exception(failure);
}

} // namespace core
