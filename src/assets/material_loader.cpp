#include "assets/material_loader.h"

#include "assets/asset_error.h"
#include "assets/asset_io.h"
#include "assets/binary.h"
#include "assets/texture_loader.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string_view>
#include <utility>

namespace CEngine::Assets
{
namespace Renderer = CEngine::Renderer;

namespace
{

constexpr std::array<char, 4> MaterialPayloadMagic = {'C', 'E', 'M', 'A'};
constexpr std::uint16_t MaterialPayloadVersion = 4;
constexpr std::uint32_t TextureSlotBaseColor = 1;
constexpr std::uint32_t TextureSlotNormal = 2;
constexpr std::uint32_t TextureSlotMetallicRoughnessAo = 3;

#pragma pack(push, 1)
struct DiskMaterialHeader
{
    std::array<char, 4> magic = MaterialPayloadMagic;
    std::uint16_t version = MaterialPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t shader = 0;
    std::uint32_t render_mode = 0;
    std::uint32_t texture_count = 0;
    std::uint32_t texture_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 0.5f;
    float ao_factor = 1.0f;
    float alpha_cutoff = 0.5f;
};

struct DiskMaterialTexture
{
    std::uint32_t slot = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskMaterialHeader) == 72, "DiskMaterialHeader must stay packed and stable.");
static_assert(sizeof(DiskMaterialTexture) == 12, "DiskMaterialTexture must stay packed and stable.");

bool ReadHeader(ByteView bytes, DiskMaterialHeader &value)
{
    if (bytes.size < sizeof(value))
    {
        return false;
    }
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    if (!ReadU16LE(bytes, offset, value.version) || !ReadU16LE(bytes, offset, value.header_size) ||
        !ReadU32LE(bytes, offset, value.shader) || !ReadU32LE(bytes, offset, value.render_mode) ||
        !ReadU32LE(bytes, offset, value.texture_count) || !ReadU32LE(bytes, offset, value.texture_table_offset) ||
        !ReadU32LE(bytes, offset, value.string_table_offset) || !ReadU32LE(bytes, offset, value.string_table_size) ||
        !ReadU32LE(bytes, offset, value.name_offset) || !ReadU32LE(bytes, offset, value.name_size))
    {
        return false;
    }
    for (float &component : value.base_color)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    return ReadF32LE(bytes, offset, value.metallic_factor) && ReadF32LE(bytes, offset, value.roughness_factor) &&
           ReadF32LE(bytes, offset, value.ao_factor) && ReadF32LE(bytes, offset, value.alpha_cutoff);
}

bool ReadTextureRow(ByteView bytes, std::size_t offset, DiskMaterialTexture &value)
{
    return ReadU32LE(bytes, offset, value.slot) && ReadU32LE(bytes, offset, value.path_offset) &&
           ReadU32LE(bytes, offset, value.path_size);
}

bool StringViewAt(ByteView string_table, std::uint32_t offset, std::uint32_t size, std::string_view &view)
{
    if (offset > string_table.size || size > string_table.size - offset)
    {
        return false;
    }
    view = std::string_view(reinterpret_cast<const char *>(string_table.data + offset), size);
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

bool MaterialRenderModeFromDisk(std::uint32_t value, Renderer::MaterialRenderMode &mode)
{
    switch (value)
    {
    case 0:
        mode = Renderer::MaterialRenderMode::OpaqueDeferred;
        return true;
    case 1:
        mode = Renderer::MaterialRenderMode::AlphaClip;
        return true;
    case 2:
        mode = Renderer::MaterialRenderMode::AlphaHashDither;
        return true;
    case 3:
        mode = Renderer::MaterialRenderMode::TransparentBlend;
        return true;
    case 4:
        mode = Renderer::MaterialRenderMode::Unlit;
        return true;
    default:
        return false;
    }
}

bool IsUnitValue(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

std::filesystem::path ProjectRootForAsset(const std::filesystem::path &asset_path)
{
    std::error_code error;
    std::filesystem::path absolute_path = std::filesystem::absolute(asset_path, error);
    if (error)
    {
        return {};
    }
    for (std::filesystem::path path = absolute_path.parent_path(); !path.empty(); path = path.parent_path())
    {
        if (path.filename() == "assets")
        {
            return path.parent_path();
        }
        if (path == path.root_path())
        {
            break;
        }
    }
    return {};
}

bool IsDdsPath(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    for (char &character : extension)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return extension == ".dds";
}

bool ResolveTexturePath(const std::filesystem::path &material_path, std::string_view stored_path,
                        std::filesystem::path &resolved_path)
{
    const std::filesystem::path raw_path(stored_path);
    const std::filesystem::path normalized = raw_path.lexically_normal();
    if (normalized.is_absolute() || normalized.empty() || normalized.begin() == normalized.end() ||
        *normalized.begin() != "assets")
    {
        return AssetError("material texture path must be project-relative and start with assets/");
    }

    for (const std::filesystem::path &component : normalized)
    {
        if (component == "..")
        {
            return AssetError("material texture path escapes the project");
        }
    }

    if (!IsDdsPath(normalized))
    {
        return AssetError("material texture dependency is not a .dds target asset");
    }

    const std::filesystem::path project_root = ProjectRootForAsset(material_path);
    if (project_root.empty())
    {
        return AssetError("material is not inside a project assets directory");
    }

    const std::filesystem::path full_path = project_root / normalized;
    resolved_path = full_path.lexically_normal();
    return true;
}

} // namespace

bool LoadMaterialAsset(const std::filesystem::path &path, Renderer::Material &material)
{
    AssetFile asset;
    if (!asset.Load(path))
    {
        return false;
    }
    if (asset.Type() != AssetType::Material)
    {
        return AssetError("asset is not a material");
    }

    const ByteView payload = asset.Payload();
    if (payload.size < sizeof(DiskMaterialHeader))
    {
        return AssetError("material payload is invalid");
    }

    DiskMaterialHeader header;
    if (!ReadHeader(payload, header))
    {
        return AssetError("material payload is invalid");
    }
    if (header.magic != MaterialPayloadMagic || header.version != MaterialPayloadVersion ||
        header.header_size != sizeof(DiskMaterialHeader))
    {
        return AssetError("material payload header is not supported");
    }
    if (!IsUnitValue(header.base_color[0]) || !IsUnitValue(header.base_color[1]) ||
        !IsUnitValue(header.base_color[2]) || !IsUnitValue(header.base_color[3]) ||
        !IsUnitValue(header.metallic_factor) || !IsUnitValue(header.roughness_factor) ||
        !IsUnitValue(header.ao_factor) || !IsUnitValue(header.alpha_cutoff))
    {
        return AssetError("material factors must be finite values from zero through one");
    }
    const Renderer::MaterialShaderType shader_type = MaterialShaderFromDisk(header.shader);
    if (shader_type == Renderer::MaterialShaderType::Unknown)
    {
        return AssetError("material shader id is not supported");
    }
    Renderer::MaterialRenderMode render_mode;
    if (!MaterialRenderModeFromDisk(header.render_mode, render_mode))
    {
        return AssetError("material render mode is not supported");
    }

    const std::size_t texture_table_size = static_cast<std::size_t>(header.texture_count) * sizeof(DiskMaterialTexture);
    if (header.texture_table_offset > payload.size || texture_table_size > payload.size - header.texture_table_offset ||
        header.string_table_offset > payload.size ||
        header.string_table_size > payload.size - header.string_table_offset)
    {
        return AssetError("material payload tables are outside the payload");
    }

    const ByteView texture_table = {payload.data + header.texture_table_offset, texture_table_size};
    const ByteView string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view material_name;
    if (!StringViewAt(string_table, header.name_offset, header.name_size, material_name))
    {
        return AssetError("material name is outside the string table");
    }

    std::string albedo_path_text;
    std::string normal_path_text;
    std::string metallic_roughness_ao_path_text;
    for (std::uint32_t index = 0; index < header.texture_count; ++index)
    {
        DiskMaterialTexture texture;
        if (!ReadTextureRow(texture_table, static_cast<std::size_t>(index) * sizeof(texture), texture))
        {
            return AssetError("material texture table is invalid");
        }

        std::string_view path_view;
        if (!StringViewAt(string_table, texture.path_offset, texture.path_size, path_view))
        {
            return AssetError("material texture path is outside the string table");
        }

        switch (texture.slot)
        {
        case TextureSlotBaseColor:
            albedo_path_text = std::string(path_view);
            break;
        case TextureSlotNormal:
            normal_path_text = std::string(path_view);
            break;
        case TextureSlotMetallicRoughnessAo:
            metallic_roughness_ao_path_text = std::string(path_view);
            break;
        default:
            break;
        }
    }

    Renderer::Material loaded;
    const auto load_texture = [&](const std::string &stored_path, Renderer::Texture &texture) {
        if (stored_path.empty())
        {
            return true;
        }
        std::filesystem::path texture_path;
        return ResolveTexturePath(path, stored_path, texture_path) && LoadTextureAsset(texture_path, texture);
    };
    if (!load_texture(albedo_path_text, loaded.albedo) || !load_texture(normal_path_text, loaded.normal) ||
        !load_texture(metallic_roughness_ao_path_text, loaded.metallic_roughness_ao))
    {
        return false;
    }

    loaded.shader_type = shader_type;
    loaded.material_name = material_name.empty() ? path.stem().string() : std::string(material_name);
    loaded.base_color_factor =
        glm::vec4(header.base_color[0], header.base_color[1], header.base_color[2], header.base_color[3]);
    loaded.metallic_roughness_ao_factors = glm::vec3(header.metallic_factor, header.roughness_factor, header.ao_factor);
    loaded.render_mode = render_mode;
    loaded.alpha_cutoff = header.alpha_cutoff;
    material = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
