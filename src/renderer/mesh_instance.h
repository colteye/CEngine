//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/mesh_instance.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_MESH_INSTANCE_H
#define CENGINE_RENDERER_MESH_INSTANCE_H

#include "handle.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/texture.h"

#include <array>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

struct MeshInstanceSlotTag;
using MeshInstanceHandle = Handle<MeshInstanceSlotTag>;

inline constexpr std::uint32_t MeshInstanceFlagCastsShadow = 1u << 0u;
inline constexpr std::uint32_t MeshInstanceFlagShadowOnly = 1u << 1u;
inline constexpr std::uint32_t MeshInstanceFlagVisible = 1u << 2u;

/**
 * @brief TODO: Describe MeshInstance.
 */
struct MeshInstance
{
    std::shared_ptr<const Mesh> mesh;
    std::shared_ptr<const Material> material;
    std::shared_ptr<const Texture> lightmap;
    glm::vec2 lightmap_scale = glm::vec2(1.0f);
    glm::vec2 lightmap_offset = glm::vec2(0.0f);
    float lightmap_rgbm_range = 8.0f;
    glm::mat4 transform = glm::mat4(1.0f);
    uint32_t flags = MeshInstanceFlagCastsShadow | MeshInstanceFlagVisible;
};

/**
 * @brief TODO: Describe Frustum.
 */
struct Frustum
{
    std::array<glm::vec4, 6> planes{};
};

/**
 * @brief TODO: Describe TransformBounds.
 *
 * @param bounds TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Bounds TransformBounds(const Bounds &bounds, const glm::mat4 &transform);
/**
 * @brief TODO: Describe ExtractFrustum.
 *
 * @param view_projection TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Frustum ExtractFrustum(const glm::mat4 &view_projection);
/**
 * @brief TODO: Describe IntersectsFrustum.
 *
 * @param bounds TODO: Describe this parameter.
 * @param frustum TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool IntersectsFrustum(const Bounds &bounds, const Frustum &frustum);

} // namespace CEngine::Renderer

#endif
