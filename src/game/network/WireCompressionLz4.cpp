#include "src/game/network/WireCompressionLz4.h"

#include <lz4.h>

namespace game::network::wire
{

bool Lz4WireCompression::compress(const std::vector<std::uint8_t>& input, std::vector<std::uint8_t>& output) const
{
    if (input.size() > MaxWirePayloadBytes)
        return false;
    const int bound = LZ4_compressBound(static_cast<int>(input.size()));
    output.resize(static_cast<std::size_t>(bound));
    const int n = LZ4_compress_default(reinterpret_cast<const char*>(input.data()), reinterpret_cast<char*>(output.data()),
                                       static_cast<int>(input.size()), bound);
    if (n <= 0)
        return false;
    output.resize(static_cast<std::size_t>(n));
    return true;
}

bool Lz4WireCompression::decompress(const std::vector<std::uint8_t>& input, std::size_t expectedUncompressedBytes,
                                    std::vector<std::uint8_t>& output) const
{
    if (expectedUncompressedBytes > MaxWirePayloadBytes)
        return false;
    output.resize(expectedUncompressedBytes);
    const int n = LZ4_decompress_safe(reinterpret_cast<const char*>(input.data()), reinterpret_cast<char*>(output.data()),
                                      static_cast<int>(input.size()), static_cast<int>(expectedUncompressedBytes));
    return n >= 0 && static_cast<std::size_t>(n) == expectedUncompressedBytes;
}

} // namespace game::network::wire
