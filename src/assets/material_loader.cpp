#include "assets/material_loader.h"

#include "assets/asset_io.h"

#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace CEngine::Assets {
namespace Renderer = CEngine::Renderer;

namespace {

constexpr std::array<char, 4> MaterialPayloadMagic = {'C', 'E', 'M', 'A'};
constexpr std::uint16_t MaterialPayloadVersion = 1;
constexpr std::uint32_t TextureSlotBaseColor = 1;
constexpr std::uint32_t TextureSlotNormal = 2;
constexpr std::uint32_t TextureSlotMetallicRoughnessAo = 3;
constexpr std::uint32_t TextureSlotRoughness = 4;
constexpr std::uint32_t TextureSlotMetallic = 5;

#pragma pack(push, 1)
struct DiskMaterialHeader {
    std::array<char, 4> magic = MaterialPayloadMagic;
    std::uint16_t version = MaterialPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t shader = 0;
    std::uint32_t texture_count = 0;
    std::uint32_t texture_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
};

struct DiskMaterialTexture {
    std::uint32_t slot = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskMaterialHeader) == 36, "DiskMaterialHeader must stay packed and stable.");
static_assert(sizeof(DiskMaterialTexture) == 12, "DiskMaterialTexture must stay packed and stable.");

void SetError(std::string* error, const std::string& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

bool StringViewAt(ByteView string_table, std::uint32_t offset, std::uint32_t size, std::string_view& view)
{
    if (offset > string_table.size || size > string_table.size - offset)
    {
        return false;
    }
    view = std::string_view(reinterpret_cast<const char*>(string_table.data + offset), size);
    return true;
}

Renderer::MaterialShaderType MaterialShaderFromDisk(std::uint32_t shader)
{
    switch (shader)
    {
    case static_cast<std::uint32_t>(Renderer::MaterialShaderType::PBRStandard):
        return Renderer::MaterialShaderType::PBRStandard;
    case static_cast<std::uint32_t>(Renderer::MaterialShaderType::Unlit):
        return Renderer::MaterialShaderType::Unlit;
    default:
        return Renderer::MaterialShaderType::Unknown;
    }
}

} // namespace

bool LoadMaterialAsset(const std::filesystem::path& path, Renderer::Material& material, std::string* error)
{
    AssetFile asset;
    if (!asset.Load(path, error))
    {
        return false;
    }
    if (asset.Type() != AssetType::Material)
    {
        SetError(error, "asset is not a material");
        return false;
    }

    const ByteView payload = asset.Payload();
    if (payload.size < sizeof(DiskMaterialHeader))
    {
        SetError(error, "material payload is invalid");
        return false;
    }

    DiskMaterialHeader header;
    std::memcpy(&header, payload.data, sizeof(header));
    if (header.magic != MaterialPayloadMagic ||
        header.version != MaterialPayloadVersion ||
        header.header_size != sizeof(DiskMaterialHeader))
    {
        SetError(error, "material payload header is not supported");
        return false;
    }
    const Renderer::MaterialShaderType shader_type = MaterialShaderFromDisk(header.shader);
    if (shader_type == Renderer::MaterialShaderType::Unknown)
    {
        SetError(error, "material shader id is not supported");
        return false;
    }

    const std::size_t texture_table_size =
        static_cast<std::size_t>(header.texture_count) * sizeof(DiskMaterialTexture);
    if (header.texture_table_offset > payload.size ||
        texture_table_size > payload.size - header.texture_table_offset ||
        header.string_table_offset > payload.size ||
        header.string_table_size > payload.size - header.string_table_offset)
    {
        SetError(error, "material payload tables are outside the payload");
        return false;
    }

    const ByteView texture_table = {payload.data + header.texture_table_offset, texture_table_size};
    const ByteView string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view material_name;
    if (!StringViewAt(string_table, header.name_offset, header.name_size, material_name))
    {
        SetError(error, "material name is outside the string table");
        return false;
    }

    std::string albedo_path;
    std::string normal_path;
    std::string metallic_roughness_ao_path;
    std::string roughness_path;
    std::string metallic_path;
    for (std::uint32_t index = 0; index < header.texture_count; ++index)
    {
        DiskMaterialTexture texture;
        std::memcpy(&texture, texture_table.data + static_cast<std::size_t>(index) * sizeof(texture), sizeof(texture));

        std::string_view path_view;
        if (!StringViewAt(string_table, texture.path_offset, texture.path_size, path_view))
        {
            SetError(error, "material texture path is outside the string table");
            return false;
        }

        switch (texture.slot)
        {
        case TextureSlotBaseColor:
            albedo_path = std::string(path_view);
            break;
        case TextureSlotNormal:
            normal_path = std::string(path_view);
            break;
        case TextureSlotMetallicRoughnessAo:
            metallic_roughness_ao_path = std::string(path_view);
            break;
        case TextureSlotRoughness:
            roughness_path = std::string(path_view);
            break;
        case TextureSlotMetallic:
            metallic_path = std::string(path_view);
            break;
        default:
            break;
        }
    }

    if (metallic_roughness_ao_path.empty())
    {
        metallic_roughness_ao_path = !roughness_path.empty() ? roughness_path : metallic_path;
    }
    Renderer::Material loaded(
        shader_type,
        albedo_path,
        normal_path,
        metallic_roughness_ao_path);
    loaded.material_name = material_name.empty() ? path.stem().string() : std::string(material_name);
    material = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
