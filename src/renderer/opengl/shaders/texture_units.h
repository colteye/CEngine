#ifndef CENGINE_RENDERER_OPENGL_SHADERS_TEXTURE_UNITS_H
#define CENGINE_RENDERER_OPENGL_SHADERS_TEXTURE_UNITS_H

#include "renderer/opengl/shadow_types.h"

#include <glad/glad.h>

namespace CEngine::Renderer::OpenGL::TextureUnits
{

// PBR programs share this non-overlapping sampler layout. OpenGL requires
// samplers of different types in one program to reference distinct units.
inline constexpr GLint MaterialFirst = 0;
inline constexpr GLint SkinningPalette = 4;
inline constexpr GLint ShadowAtlas = 5;
inline constexpr GLint PointShadowFirst = 6;
inline constexpr GLint EnvironmentProbeFirst = PointShadowFirst + ShadowLimits::KMaxPointShadows;
inline constexpr GLint EnvironmentProbeCount = 4;
inline constexpr GLint GlobalIrradiance = EnvironmentProbeFirst + EnvironmentProbeCount;
inline constexpr GLint GlobalPrefiltered = GlobalIrradiance + 1;
inline constexpr GLint RequiredUnitCount = GlobalPrefiltered + 1;

static_assert(SkinningPalette == MaterialFirst + 4);
static_assert(EnvironmentProbeFirst == 10);
static_assert(RequiredUnitCount == 16);

} // namespace CEngine::Renderer::OpenGL::TextureUnits

#endif
