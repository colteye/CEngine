#ifndef CENGINE_RENDERER_PARTICLE_EMITTER_H
#define CENGINE_RENDERER_PARTICLE_EMITTER_H

#include "assets/particle_asset.h"
#include "handle.h"
#include "renderer/texture.h"

#include <cstdint>
#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace CEngine::Renderer
{

struct ParticleEmitterSlotTag;
using ParticleEmitterHandle = Handle<ParticleEmitterSlotTag>;

inline constexpr std::uint32_t ParticleFlagLoop = 1u << 0u;
inline constexpr std::uint32_t ParticleFlagWorldSpace = 1u << 1u;

enum class ParticleBlendMode : std::uint32_t
{
    Alpha = 0,
    Additive = 1,
    PremultipliedAlpha = 2,
};

/**
 * One retained placement and capacity bound for an immutable particle effect.
 * The optional texture is resolved separately from the effect's asset
 * reference, just like textures retained by mesh instances.
 */
struct ParticleEmitter
{
    std::shared_ptr<const Assets::Particle> particle;
    std::shared_ptr<const Texture> texture;
    glm::mat4 transform = glm::mat4(1.0f);
    std::uint32_t capacity = 256;
    bool emitting = true;
};

/**
 * Backend-neutral, fully simulated particle presented as a camera-facing
 * billboard by graphics backends that support the particle path.
 */
struct ParticleDraw
{
    glm::vec3 position = glm::vec3(0.0f);
    float size = 0.0f;
    glm::vec4 color = glm::vec4(1.0f);
    const Texture *texture = nullptr;
    ParticleBlendMode blend_mode = ParticleBlendMode::Alpha;
};

} // namespace CEngine::Renderer

#endif
