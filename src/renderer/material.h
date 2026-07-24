//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/material.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef MATERIAL_H
#define MATERIAL_H

#include "renderer/texture.h"

#include <glm/glm.hpp>

#include <string>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe MaterialShaderType.
 */
enum class MaterialShaderType : unsigned int
{
    Unknown = 0,
    PBRStandard = 1,
    Unlit = 2,
};

/**
 * @brief TODO: Describe MaterialRenderMode.
 */
enum class MaterialRenderMode
{
    OpaqueDeferred,
    AlphaClip,
    AlphaHashDither,
    TransparentBlend,
    Unlit
};

/**
 * @brief TODO: Describe RequiresAlphaTest.
 *
 * @param mode TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
constexpr bool RequiresAlphaTest(MaterialRenderMode mode)
{
    return mode == MaterialRenderMode::AlphaClip || mode == MaterialRenderMode::AlphaHashDither;
}

/**
 * @brief TODO: Describe Material.
 */
struct Material
{
    std::string material_name;
    MaterialShaderType shader_type = MaterialShaderType::PBRStandard;
    Texture albedo;
    Texture normal;
    Texture metallic_roughness_ao;
    MaterialRenderMode render_mode = MaterialRenderMode::OpaqueDeferred;
    glm::vec4 base_color_factor = glm::vec4(1.0f);
    glm::vec3 metallic_roughness_ao_factors = glm::vec3(0.0f, 0.5f, 1.0f);
    float alpha_cutoff = 0.5f;
    bool receives_shadows = true;
    bool casts_shadows = true;
};

} // namespace CEngine::Renderer

#endif
