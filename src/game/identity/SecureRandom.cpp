#include "src/game/identity/SecureRandom.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace game::identity
{
bool fillSecureRandom(
    std::uint8_t* data,
    std::size_t size
) noexcept
{
    if (!data && size != 0)
        return false;

#ifdef _WIN32
    if (size > static_cast<std::size_t>(ULONG_MAX))
        return false;

    return BCryptGenRandom(
               nullptr,
               reinterpret_cast<PUCHAR>(data),
               static_cast<ULONG>(size),
               BCRYPT_USE_SYSTEM_PREFERRED_RNG
           ) == 0;
#else
    const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;

    std::size_t offset = 0;
    while (offset < size)
    {
        const ssize_t readCount = ::read(fd, data + offset, size - offset);
        if (readCount > 0)
        {
            offset += static_cast<std::size_t>(readCount);
            continue;
        }
        if (readCount < 0 && errno == EINTR)
            continue;
        ::close(fd);
        return false;
    }

    ::close(fd);
    return true;
#endif
}
}
