#ifndef OPENGL_SHADOW_TYPES_H
#define OPENGL_SHADOW_TYPES_H

#include <array>

#include <glm/glm.hpp>

namespace OpenGLShadows {
constexpr int kAtlasSize = 4096;
constexpr int kMaxSpotShadows = 16;
constexpr int kCascadeCount = 4;
constexpr int kMaxDirectionalLights = 2;
constexpr int kMaxDirectionalCascades = kCascadeCount * kMaxDirectionalLights;
constexpr int kMaxPointShadows = 8;
constexpr int kTypeNone = 0;
constexpr int kTypeSpot = 1;
constexpr int kTypeDirectional = 2;
constexpr int kTypePoint = 3;
} // namespace OpenGLShadows

struct OpenGLShadowGpuData
{
	std::array<glm::mat4, OpenGLShadows::kMaxSpotShadows> spot_matrices {};
	std::array<glm::mat4, OpenGLShadows::kMaxDirectionalCascades> cascade_matrices {};
	std::array<glm::vec4, OpenGLShadows::kMaxSpotShadows> spot_atlas_rects {};
	std::array<glm::vec4, OpenGLShadows::kMaxSpotShadows> spot_params {};
	std::array<glm::vec4, OpenGLShadows::kMaxDirectionalCascades> cascade_atlas_rects {};
	std::array<glm::vec4, OpenGLShadows::kMaxDirectionalCascades> cascade_params {};
	std::array<glm::vec4, OpenGLShadows::kMaxPointShadows> point_params {};
	std::array<glm::vec4, OpenGLShadows::kMaxPointShadows> point_shadow_params {};
	glm::vec4 shadow_counts = glm::vec4(0.0f);
};

#endif
