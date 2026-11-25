#pragma once

// LZ4 for the data plane (snapshots): measured 2026-11-25 the wire had NO compression at all (WireCompressionMethod::None
// was the only one) - an EMPTY ship row cost 4185 B, a snapshot 24 KB, 379 KiB/s a player. LZ4 is fast enough for 16
// snapshots a second per player on the server and decodes far faster than that on the client. Static liblz4 (no DLL).

#include "src/game/network/WireCompression.h"

namespace game::network::wire
{

class Lz4WireCompression final : public IWireCompressor
{
public:
    WireCompressionMethod method() const noexcept override { return WireCompressionMethod::Lz4; }
    bool compress(const std::vector<std::uint8_t>& input, std::vector<std::uint8_t>& output) const override;
    bool decompress(const std::vector<std::uint8_t>& input, std::size_t expectedUncompressedBytes,
                    std::vector<std::uint8_t>& output) const override;
};

} // namespace game::network::wire
