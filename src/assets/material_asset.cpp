//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/material_asset.cpp
 * @brief Instantiates a renderer material from generated wire data.
 * @author Erik Coltey
 */

#include "assets/material_asset.h"

#include "assets/texture_asset.h"
#include "engine/engine_entities.generated.h"
#include "log.h"

#include <array>
#include <cctype>

namespace CEngine::Assets
{
namespace
{
namespace Wire = Generated::EngineEntities::Wire;

bool ResolveTexture(const std::filesystem::path &material, std::string_view stored, std::filesystem::path &output)
{
    std::filesystem::path path(stored);
    if (path.is_absolute())
    {
        output = path.lexically_normal();
        return true;
    }
    for (std::filesystem::path root = material.parent_path(); !root.empty(); root = root.parent_path())
    {
        if (root.filename() == "assets")
        {
            output = (root.parent_path() / path).lexically_normal();
            return true;
        }
        if (root == root.root_path())
        {
            break;
        }
    }
    output = (material.parent_path() / path).lexically_normal();
    return true;
}

Renderer::MaterialRenderMode RenderMode(std::uint32_t value)
{
    return static_cast<Renderer::MaterialRenderMode>(value);
}
} // namespace

bool LoadMaterialAsset(const std::filesystem::path &path, Renderer::Material &material)
{
    Wire::Material decoded;
    if (!Decode(path, Type::Material, decoded))
    {
        Logging::Logger::Get().Error("assets", "material payload is invalid");
        return false;
    }

    Renderer::Material loaded;
    loaded.material_name = std::move(decoded.name);
    loaded.shader_type = static_cast<Renderer::MaterialShaderType>(decoded.shader);
    loaded.render_mode = RenderMode(decoded.render_mode);
    loaded.base_color_factor = {
        decoded.base_color[0],
        decoded.base_color[1],
        decoded.base_color[2],
        decoded.base_color[3],
    };
    loaded.metallic_roughness_ao_factors = {decoded.metallic, decoded.roughness, decoded.ao};
    loaded.alpha_cutoff = decoded.alpha_cutoff;

    std::array<bool, 4> slots{};
    for (const Wire::MaterialTexture &texture : decoded.textures)
    {
        if (texture.slot >= slots.size() || slots[texture.slot] || texture.asset.type != 1u)
        {
            Logging::Logger::Get().Error("assets", "material texture reference is invalid");
            return false;
        }
        slots[texture.slot] = true;
        std::filesystem::path texture_path;
        if (!ResolveTexture(path, texture.asset.path, texture_path))
        {
            return false;
        }
        Renderer::Texture *target = texture.slot == 1u   ? &loaded.albedo
                                    : texture.slot == 2u ? &loaded.normal
                                                         : &loaded.metallic_roughness_ao;
        if (!LoadTextureAsset(texture_path, *target))
        {
            return false;
        }
    }

    material = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
