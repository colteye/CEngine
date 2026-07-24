#include "assets/particle_asset.h"
#include "renderer/particle_emitter.h"
#include "renderer/render_system.h"

#include <cmath>
#include <iostream>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

namespace
{

bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

std::shared_ptr<const CEngine::Assets::Particle> MakeParticle(std::uint32_t flags)
{
    auto particle = std::make_shared<CEngine::Assets::Particle>();
    particle->name = "TestParticle";
    particle->flags = flags;
    particle->blend_mode = static_cast<std::uint32_t>(CEngine::Renderer::ParticleBlendMode::Additive);
    particle->rate = 10.0f;
    particle->lifetime = {1.0f, 1.0f};
    particle->speed = {2.0f, 2.0f};
    particle->spread = 0.0f;
    particle->gravity = {0.0f, 0.0f, -1.0f};
    particle->size = {1.0f, 0.5f};
    particle->start_color = {1.0f, 0.5f, 0.0f, 1.0f};
    particle->end_color = {0.0f, 0.0f, 1.0f, 0.0f};
    return particle;
}

bool BurstIsBoundedAndSimulated()
{
    CEngine::Renderer::RenderSystem rendering;
    CEngine::Renderer::ParticleEmitter emitter;
    emitter.particle = MakeParticle(CEngine::Renderer::ParticleFlagWorldSpace);
    emitter.transform = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 4.0f, 5.0f));
    emitter.capacity = 2;
    CEngine::Renderer::ParticleEmitter oversized = emitter;
    oversized.capacity = static_cast<std::uint32_t>(CEngine::Renderer::RenderSystem::MaxParticlesPerEmitter + 1u);
    if (!Expect(!rendering.RegisterParticleEmitter(oversized), "particle emitters should reject unbounded capacity"))
    {
        return false;
    }
    emitter.emitting = false;
    const CEngine::Renderer::ParticleEmitterHandle handle = rendering.RegisterParticleEmitter(emitter);
    if (!Expect(static_cast<bool>(handle), "valid particle emitter should register") ||
        !Expect(rendering.EmitParticles(handle, 3) == 2, "burst should stop at the emitter capacity") ||
        !Expect(rendering.GetParticleDraws().size() == 2, "burst particles should be submitted for rendering"))
    {
        return false;
    }

    const CEngine::Renderer::ParticleDraw &spawned = rendering.GetParticleDraws().front();
    const bool starts_at_emitter =
        Expect(Near(spawned.position.x, 3.0f) && Near(spawned.position.y, 4.0f) && Near(spawned.position.z, 5.0f),
               "world-space particles should start at the emitter transform");

    rendering.UpdateParticles(0.25f);
    const CEngine::Renderer::ParticleDraw &advanced = rendering.GetParticleDraws().front();
    const bool simulated =
        Expect(Near(advanced.position.x, 3.5f) && Near(advanced.position.z, 4.9375f) && Near(advanced.size, 0.875f) &&
                   Near(advanced.color.r, 0.75f) && Near(advanced.color.b, 0.25f),
               "particle position, gravity, size, and color should advance together");

    rendering.UpdateParticleEmitter(handle, emitter.transform, false);
    rendering.UpdateParticles(0.75f);
    const bool expired = Expect(rendering.GetParticleCount(handle) == 0 && rendering.GetParticleDraws().empty(),
                                "particles should be removed at their configured lifetime");
    rendering.RemoveParticleEmitter(handle);
    const bool stale = Expect(rendering.ResolveParticleEmitter(handle) == nullptr &&
                                  rendering.EmitParticles(handle, 1) == 0 && rendering.GetParticleEmitterCount() == 0,
                              "removed particle emitter handles should be stale");
    const CEngine::Renderer::ParticleEmitterHandle replacement = rendering.RegisterParticleEmitter(emitter);
    const bool reused = Expect(replacement && replacement != handle && rendering.EmitParticles(replacement, 1) == 1,
                               "a reused emitter slot should publish a new generation");
    return starts_at_emitter && simulated && expired && stale && reused;
}

bool LoopingEmissionUsesRateAndLocalSpace()
{
    CEngine::Renderer::RenderSystem rendering;
    CEngine::Renderer::ParticleEmitter emitter;
    emitter.particle = MakeParticle(CEngine::Renderer::ParticleFlagLoop);
    emitter.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 1.0f, 0.5f));
    emitter.capacity = 8;
    const CEngine::Renderer::ParticleEmitterHandle handle = rendering.RegisterParticleEmitter(emitter);
    if (!Expect(static_cast<bool>(handle), "valid looping particle emitter should register"))
    {
        return false;
    }

    rendering.UpdateParticles(0.25f);
    const bool rate = Expect(rendering.GetParticleCount(handle) == 2,
                             "looping emitters should accumulate their configured particles-per-second rate");
    const CEngine::Renderer::ParticleDraw &draw = rendering.GetParticleDraws().front();
    const bool local =
        Expect(Near(draw.position.x, -2.0f) && Near(draw.position.y, 1.0f) && Near(draw.position.z, 0.5f),
               "local-space particles should be transformed by their emitter");

    rendering.UpdateParticleEmitter(handle, glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 0.0f, 0.0f)), false);
    const bool follows = Expect(Near(rendering.GetParticleDraws().front().position.x, 4.0f),
                                "local-space particles should follow emitter transform changes");
    return rate && local && follows;
}

} // namespace

int main()
{
    return BurstIsBoundedAndSimulated() && LoopingEmissionUsesRateAndLocalSpace() ? 0 : 1;
}
