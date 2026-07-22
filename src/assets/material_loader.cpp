#include "assets/material_loader.h"

#include "assets/asset_io.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string_view>
#include <utility>

namespace CEngine::Assets {
namespace Renderer = CEngine::Renderer;

namespace {

constexpr std::array<char, 4> MaterialPayloadMagic = {'C', 'E', 'M', 'A'};
constexpr std::uint16_t MaterialPayloadVersion = 4;
constexpr std::uint32_t TextureSlotBaseColor = 1;
constexpr std::uint32_t TextureSlotNormal = 2;
constexpr std::uint32_t TextureSlotMetallicRoughnessAo = 3;

#pragma pack(push, 1)
struct DiskMaterialHeader {
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

struct DiskMaterialTexture {
    std::uint32_t slot = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskMaterialHeader) == 72, "DiskMaterialHeader must stay packed and stable.");
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

bool MaterialRenderModeFromDisk(std::uint32_t value, Renderer::MaterialRenderMode& mode)
{
    switch (value)
    {
    case 0: mode = Renderer::MaterialRenderMode::OpaqueDeferred; return true;
    case 1: mode = Renderer::MaterialRenderMode::AlphaClip; return true;
    case 2: mode = Renderer::MaterialRenderMode::AlphaHashDither; return true;
    case 3: mode = Renderer::MaterialRenderMode::TransparentBlend; return true;
    case 4: mode = Renderer::MaterialRenderMode::Unlit; return true;
    default: return false;
    }
}

bool IsUnitValue(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

std::filesystem::path ProjectRootForAsset(const std::filesystem::path& asset_path)
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

std::string ExistingGenericPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

bool IsDdsPath(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    for (char& character : extension)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return extension == ".dds";
}

bool HasDdsMagic(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::array<char, 4> magic{};
    return stream.read(magic.data(), static_cast<std::streamsize>(magic.size())) &&
        magic == std::array<char, 4>{'D', 'D', 'S', ' '};
}

bool ResolveTexturePath(const std::filesystem::path& material_path, std::string_view stored_path,
    std::string& resolved_path, std::string* error)
{
    const std::filesystem::path raw_path(stored_path);
    const std::filesystem::path normalized = raw_path.lexically_normal();
    if (normalized.is_absolute() || normalized.empty() || normalized.begin() == normalized.end() ||
        *normalized.begin() != "assets")
    {
        SetError(error, "material texture path must be project-relative and start with assets/");
        return false;
    }

    for (const std::filesystem::path& component : normalized)
    {
        if (component == "..")
        {
            SetError(error, "material texture path escapes the project");
            return false;
        }
    }

    if (!IsDdsPath(normalized))
    {
        SetError(error, "material texture dependency is not a .dds target asset");
        return false;
    }

    const std::filesystem::path project_root = ProjectRootForAsset(material_path);
    if (project_root.empty())
    {
        SetError(error, "material is not inside a project assets directory");
        return false;
    }

    const std::filesystem::path full_path = project_root / normalized;
    if (!std::filesystem::is_regular_file(full_path))
    {
        SetError(error, "material texture dependency does not exist: " + ExistingGenericPath(full_path));
        return false;
    }
    if (!HasDdsMagic(full_path))
    {
        SetError(error, "material texture dependency is not a DDS file: " + ExistingGenericPath(full_path));
        return false;
    }
    resolved_path = ExistingGenericPath(full_path);
    return true;
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
    if (!IsUnitValue(header.base_color[0]) || !IsUnitValue(header.base_color[1]) ||
        !IsUnitValue(header.base_color[2]) || !IsUnitValue(header.base_color[3]) ||
        !IsUnitValue(header.metallic_factor) || !IsUnitValue(header.roughness_factor) ||
        !IsUnitValue(header.ao_factor) || !IsUnitValue(header.alpha_cutoff))
    {
        SetError(error, "material factors must be finite values from zero through one");
        return false;
    }
    const Renderer::MaterialShaderType shader_type = MaterialShaderFromDisk(header.shader);
    if (shader_type == Renderer::MaterialShaderType::Unknown)
    {
        SetError(error, "material shader id is not supported");
        return false;
    }
    Renderer::MaterialRenderMode render_mode;
    if (!MaterialRenderModeFromDisk(header.render_mode, render_mode))
    {
        SetError(error, "material render mode is not supported");
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
        default:
            break;
        }
    }

    if ((!albedo_path.empty() && !ResolveTexturePath(path, albedo_path, albedo_path, error)) ||
        (!normal_path.empty() && !ResolveTexturePath(path, normal_path, normal_path, error)) ||
        (!metallic_roughness_ao_path.empty() &&
            !ResolveTexturePath(path, metallic_roughness_ao_path, metallic_roughness_ao_path, error)))
    {
        return false;
    }
    Renderer::Material loaded(
        shader_type,
        albedo_path,
        normal_path,
        metallic_roughness_ao_path);
    loaded.material_name = material_name.empty() ? path.stem().string() : std::string(material_name);
    loaded.SetBaseColorFactor(glm::vec4(header.base_color[0], header.base_color[1],
        header.base_color[2], header.base_color[3]));
    loaded.SetMetallicRoughnessAoFactors(glm::vec3(
        header.metallic_factor, header.roughness_factor, header.ao_factor));
    loaded.SetRenderMode(render_mode);
    loaded.SetAlphaCutoff(header.alpha_cutoff);
    material = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
