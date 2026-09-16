#include "src/model_asset/binary/ModelAssetBinaryManifestIO.h"
#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace elite::model_asset::binary
{
namespace
{
constexpr std::array<char, 8> ManifestMagicV4 {{'E','L','M','D','L','0','0','4'}};
constexpr std::array<char, 8> ManifestMagicV3 {{'E','L','M','D','L','0','0','3'}};
constexpr std::array<char, 8> LegacyMagicV2   {{'E','L','M','D','L','0','0','2'}};

bool readChunks(
    std::istream& file,
    std::uint32_t chunkCount,
    ModelAsset& result,
    std::uint32_t formatVersion,
    std::string* error)
{
    for (std::uint32_t i = 0; i < chunkCount; ++i)
    {
        std::array<char, 4> id {};
        std::uint64_t size = 0;
        file.read(id.data(), 4);
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!file || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        {
            setError(error, "corrupt model asset chunk header");
            return false;
        }

        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (size)
            file.read(bytes.data(), static_cast<std::streamsize>(size));
        if (!file)
        {
            setError(error, "truncated model asset chunk");
            return false;
        }

        ChunkReader readerFn = nullptr;
        if (formatVersion == 2u)
            readerFn = findLegacyChunkReader(id);
        else if (const ChunkSpec* spec = findManifestChunk(id, formatVersion >= 4u))
            readerFn = spec->reader;
        if (!readerFn)
            continue;

        std::istringstream payload(bytes, std::ios::binary);
        Reader reader {payload};
        readerFn(reader, result);
        if (!reader.ok)
        {
            setError(error, "corrupt model asset chunk payload");
            return false;
        }
    }
    return true;
}
}

bool writeManifestV4(const std::filesystem::path& path, const ModelAsset& asset, std::string* error)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        setError(error, "cannot open output manifest: " + path.string());
        return false;
    }

    file.write(ManifestMagicV4.data(), static_cast<std::streamsize>(ManifestMagicV4.size()));
    Writer header {file};
    header.pod(ModelAssetFormatVersion);
    header.pod(static_cast<std::uint32_t>(manifestChunksV4().size()));
    if (!header.ok)
    {
        setError(error, "failed writing model asset manifest header");
        return false;
    }

    for (const auto& chunk : manifestChunksV4())
    {
        std::ostringstream payload(std::ios::binary);
        Writer writer {payload};
        chunk.writer(writer, asset);
        if (!writer.ok)
        {
            setError(error, "failed to serialize model asset manifest chunk");
            return false;
        }
        const std::string bytes = payload.str();
        file.write(chunk.id.data(), 4);
        const std::uint64_t size = static_cast<std::uint64_t>(bytes.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!file)
        {
            setError(error, "failed writing model asset manifest: " + path.string());
            return false;
        }
    }
    return true;
}

bool readManifestPackage(
    const std::filesystem::path& path,
    ModelAsset& asset,
    bool* legacyPackage,
    std::string* error)
{
    if (legacyPackage) *legacyPackage = false;
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        setError(error, "cannot open model asset: " + path.string());
        return false;
    }

    std::array<char, 8> magic {};
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!file || (magic != ManifestMagicV4 && magic != ManifestMagicV3 && magic != LegacyMagicV2))
    {
        setError(error, "invalid model asset magic");
        return false;
    }

    Reader header {file};
    std::uint32_t version = 0, chunkCount = 0;
    header.pod(version); header.pod(chunkCount);
    const bool isV4 = magic == ManifestMagicV4 && version == 4u;
    const bool isV3 = magic == ManifestMagicV3 && version == 3u;
    const bool isV2 = magic == LegacyMagicV2 && version == 2u;
    if (!header.ok || (!isV4 && !isV3 && !isV2) || chunkCount > 1024u)
    {
        setError(error, "unsupported model asset version");
        return false;
    }

    ModelAsset result;
    result.formatVersion = version;
    if (!readChunks(file, chunkCount, result, version, error))
        return false;
    if (result.assetId.empty())
    {
        setError(error, "model asset has no asset id");
        return false;
    }

    if (isV4)
    {
        for (std::size_t i = 0; i < result.renderLods.size(); ++i)
            result.renderLods[i].level = static_cast<std::uint32_t>(i);
    }
    if (legacyPackage) *legacyPackage = !isV4;
    asset = std::move(result);
    return true;
}

} // namespace elite::model_asset::binary
