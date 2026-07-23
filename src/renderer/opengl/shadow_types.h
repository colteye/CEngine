//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shadow_types.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADOW_TYPES_H
#define CENGINE_RENDERER_OPENGL_SHADOW_TYPES_H

#include <array>

#include <glm/glm.hpp>

namespace CEngine::Renderer::OpenGL
{

namespace ShadowLimits
{
constexpr int KAtlasSize = 4096;
constexpr int KMaxSpotShadows = 16;
constexpr int KCascadeCount = 4;
constexpr int KMaxDirectionalLights = 2;
constexpr int KMaxDirectionalCascades = KCascadeCount * KMaxDirectionalLights;
// Keep four fragment texture units free on the OpenGL 3.3 minimum sampler
// budget after material, shadow-atlas, and IBL bindings.
constexpr int KMaxPointShadows = 4;
constexpr int KTypeNone = 0;
constexpr int KTypeSpot = 1;
constexpr int KTypeDirectional = 2;
constexpr int KTypePoint = 3;
} // namespace ShadowLimits

/**
 * @brief TODO: Describe ShadowGpuData.
 */
struct ShadowGpuData
{
    std::array<glm::mat4, ShadowLimits::KMaxSpotShadows> spot_matrices{};
    std::array<glm::mat4, ShadowLimits::KMaxDirectionalCascades> cascade_matrices{};
    std::array<glm::vec4, ShadowLimits::KMaxSpotShadows> spot_atlas_rects{};
    std::array<glm::vec4, ShadowLimits::KMaxSpotShadows> spot_params{};
    std::array<glm::vec4, ShadowLimits::KMaxDirectionalCascades> cascade_atlas_rects{};
    std::array<glm::vec4, ShadowLimits::KMaxDirectionalCascades> cascade_params{};
    std::array<glm::vec4, ShadowLimits::KMaxPointShadows> point_params{};
    std::array<glm::vec4, ShadowLimits::KMaxPointShadows> point_shadow_params{};
    glm::vec4 shadow_counts = glm::vec4(0.0f);
};

} // namespace CEngine::Renderer::OpenGL
#endif
