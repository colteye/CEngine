#include "assets/asset_format.h"

#include <array>

namespace CEngine::Assets {
namespace {

constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

std::uint8_t HexValue(char value)
{
    if (value >= '0' && value <= '9')
    {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f')
    {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F')
    {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    return 0xff;
}

std::uint64_t HashAppend(std::uint64_t hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= FnvPrime;
    }
    return hash;
}

} // namespace

const char* ToString(AssetType type)
{
    switch (type)
    {
    case AssetType::Texture:
        return "texture";
    case AssetType::Material:
        return "material";
    case AssetType::Mesh:
        return "mesh";
    case AssetType::Skeleton:
        return "skeleton";
    case AssetType::Animation:
        return "animation";
    case AssetType::Physics:
        return "physics";
    case AssetType::Prefab:
        return "prefab";
    case AssetType::Scene:
        return "scene";
    case AssetType::Audio:
        return "audio";
    case AssetType::Vfx:
        return "vfx";
    case AssetType::Navigation:
        return "navigation";
    case AssetType::Shader:
        return "shader";
    case AssetType::Package:
        return "package";
    case AssetType::Asset:
        return "asset";
    case AssetType::Unknown:
    default:
        return "unknown";
    }
}

const char* RuntimeExtensionFor(AssetType type)
{
    switch (type)
    {
    case AssetType::Texture:
        return "";
    case AssetType::Material:
        return ".cmat";
    case AssetType::Mesh:
        return ".cmesh";
    case AssetType::Skeleton:
        return ".cskel";
    case AssetType::Animation:
        return ".canim";
    case AssetType::Physics:
        return ".cphys";
    case AssetType::Prefab:
    case AssetType::Scene:
        return ".casset";
    case AssetType::Audio:
        return "";
    case AssetType::Vfx:
        return ".cvfx";
    case AssetType::Navigation:
        return ".cnav";
    case AssetType::Shader:
        return ".cshader";
    case AssetType::Package:
        return ".cpak";
    case AssetType::Asset:
        return ".casset";
    case AssetType::Unknown:
    default:
        return "";
    }
}

std::uint64_t HashBytes(const void* data, std::size_t size)
{
    return HashAppend(FnvOffset, data, size);
}

Guid GuidFromStableName(std::string_view name)
{
    const std::uint64_t first = HashBytes(name.data(), name.size());
    constexpr std::string_view salt = "CEngineAssetGuid";
    const std::uint64_t salted = HashAppend(FnvOffset, salt.data(), salt.size());
    const std::uint64_t second = HashAppend(salted, name.data(), name.size());

    Guid guid = {};
    for (std::size_t index = 0; index < 8; ++index)
    {
        guid[index] = static_cast<std::uint8_t>((first >> (index * 8)) & 0xff);
        guid[index + 8] = static_cast<std::uint8_t>((second >> (index * 8)) & 0xff);
    }
    return guid;
}

std::string GuidToString(const Guid& guid)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);
    for (std::size_t index = 0; index < guid.size(); ++index)
    {
        if (index == 4 || index == 6 || index == 8 || index == 10)
        {
            text.push_back('-');
        }
        text.push_back(hex[(guid[index] >> 4) & 0xf]);
        text.push_back(hex[guid[index] & 0xf]);
    }
    return text;
}

bool GuidFromString(std::string_view text, Guid& guid)
{
    std::array<char, 32> compact = {};
    std::size_t compact_size = 0;
    for (const char value : text)
    {
        if (value == '-')
        {
            continue;
        }
        if (compact_size >= compact.size())
        {
            return false;
        }
        compact[compact_size++] = value;
    }

    if (compact_size != compact.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < guid.size(); ++index)
    {
        const std::uint8_t high = HexValue(compact[index * 2]);
        const std::uint8_t low = HexValue(compact[index * 2 + 1]);
        if (high == 0xff || low == 0xff)
        {
            return false;
        }
        guid[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

} // namespace CEngine::Assets
