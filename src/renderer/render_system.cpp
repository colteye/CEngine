//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/render_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/render_system.h"
#include "window/window_system.h"

#ifdef CENGINE_ENABLE_OPENGL
#include "renderer/opengl/render_backend.h"
#endif

#ifdef CENGINE_ENABLE_VULKAN
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace
{
bool Finite(float value)
{
    return std::isfinite(value);
}

bool FiniteTransform(const glm::mat4 &transform)
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (!Finite(transform[column][row]))
            {
                return false;
            }
        }
    }
    return true;
}

bool ValidEnvironmentProbe(const CEngine::Renderer::EnvironmentProbe &probe)
{
    if (probe.panorama == nullptr || probe.panorama->Empty() || !FiniteTransform(probe.transform) ||
        !Finite(probe.blend_distance) || probe.blend_distance < 0.0f || !Finite(probe.intensity) ||
        probe.intensity < 0.0f)
    {
        return false;
    }
    return glm::length(glm::vec3(probe.transform[0])) > 0.0001f &&
           glm::length(glm::vec3(probe.transform[1])) > 0.0001f &&
           glm::length(glm::vec3(probe.transform[2])) > 0.0001f &&
           std::abs(glm::determinant(glm::mat3(probe.transform))) > 0.000001f;
}

bool SameEnvironmentProbe(const CEngine::Renderer::EnvironmentProbe &left,
                          const CEngine::Renderer::EnvironmentProbe &right)
{
    return left.panorama == right.panorama && left.transform == right.transform &&
           left.blend_distance == right.blend_distance && left.intensity == right.intensity &&
           left.enabled == right.enabled;
}

bool ValidParticleEmitter(const CEngine::Renderer::ParticleEmitter &emitter)
{
    if (emitter.particle == nullptr || emitter.capacity == 0 ||
        emitter.capacity > CEngine::Renderer::RenderSystem::MaxParticlesPerEmitter ||
        !FiniteTransform(emitter.transform) || (emitter.texture != nullptr && emitter.texture->Empty()))
    {
        return false;
    }

    const CEngine::Assets::Particle &particle = *emitter.particle;
    const auto finite_values = [](const auto &values) {
        return std::all_of(values.begin(), values.end(), [](float value) { return Finite(value); });
    };
    const auto valid_color = [&](const auto &color) {
        return finite_values(color) &&
               std::all_of(color.begin(), color.end(), [](float value) { return value >= 0.0f && value <= 1.0f; });
    };
    return particle.flags <= (CEngine::Renderer::ParticleFlagLoop | CEngine::Renderer::ParticleFlagWorldSpace) &&
           particle.blend_mode <=
               static_cast<std::uint32_t>(CEngine::Renderer::ParticleBlendMode::PremultipliedAlpha) &&
           Finite(particle.rate) && particle.rate >= 0.0f && finite_values(particle.lifetime) &&
           particle.lifetime[0] >= 0.0f && particle.lifetime[0] <= particle.lifetime[1] &&
           finite_values(particle.speed) && particle.speed[0] <= particle.speed[1] && Finite(particle.spread) &&
           particle.spread >= 0.0f && particle.spread <= std::numbers::pi_v<float> && finite_values(particle.gravity) &&
           finite_values(particle.size) && particle.size[0] >= 0.0f && particle.size[1] >= 0.0f &&
           valid_color(particle.start_color) && valid_color(particle.end_color);
}

std::uint32_t Hash(std::uint32_t value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    return value ^ (value >> 16u);
}

float RandomUnit(std::uint32_t &state)
{
    state = Hash(state);
    return static_cast<float>(state >> 8u) * (1.0f / 16777216.0f);
}

/**
 * @brief TODO: Describe SameLight.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool SameLight(const CEngine::Renderer::Light &left, const CEngine::Renderer::Light &right)
{
    return left.type == right.type && left.position == right.position && left.direction == right.direction &&
           left.color == right.color && left.intensity == right.intensity && left.range == right.range &&
           left.spot_inner_cos == right.spot_inner_cos && left.spot_outer_cos == right.spot_outer_cos &&
           left.enabled == right.enabled && left.casts_shadows == right.casts_shadows &&
           left.shadow_resolution == right.shadow_resolution && left.shadow_bias == right.shadow_bias &&
           left.shadow_normal_bias == right.shadow_normal_bias;
}
} // namespace

namespace CEngine::Renderer
{

static_assert(sizeof(GpuLight) == sizeof(glm::vec4) * 4, "GpuLight must stay tightly packed as four vec4 lanes.");

/**
 * @brief TODO: Describe RenderSystem::~RenderSystem.
 */
RenderSystem::~RenderSystem()
{
    Shutdown();
}

/**
 * @brief TODO: Describe RenderSystem::Initialize.
 *
 * @param window TODO: Describe this parameter.
 * @param window_width TODO: Describe this parameter.
 * @param window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderSystem::Initialize(Window::WindowSystem &window)
{
    Shutdown();
    const glm::ivec2 drawable_size = window.DrawableSize();
    if (drawable_size.x <= 0 || drawable_size.y <= 0)
    {
        return false;
    }

#ifdef CENGINE_ENABLE_OPENGL
    backend_ = std::make_unique<OpenGL::RenderBackend>();
#elif defined(CENGINE_ENABLE_VULKAN)
    backend_ = std::make_unique<VulkanRenderBackend>();
#endif

    if (backend_ == nullptr || !backend_->Initialize(*this, window, drawable_size.x, drawable_size.y))
    {
        backend_.reset();
        return false;
    }

    gpu_lights_.reserve(MaxGpuLights);
    return true;
}

/**
 * @brief TODO: Describe RenderSystem::Shutdown.
 */
void RenderSystem::Shutdown()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
        backend_.reset();
    }

    mesh_instances_.clear();
    mesh_instance_local_bounds_.clear();
    mesh_instance_world_bounds_.clear();
    mesh_instance_generations_.clear();
    free_mesh_instances_.clear();
    mesh_instance_revision_ = 1;
    particle_emitters_.clear();
    particle_emitter_generations_.clear();
    free_particle_emitters_.clear();
    particle_draws_.clear();
    particle_emitter_count_ = 0;
    environment_probes_.clear();
    environment_probe_generations_.clear();
    free_environment_probes_.clear();
    environment_probe_revision_ = 1;
    direct_lights_.clear();
    light_generations_.clear();
    free_lights_.clear();
    gpu_lights_.clear();
    light_shadow_handles_.clear();
    camera_frame_data_ = {};
    active_camera_ = {};
    camera_aspect_ratio_ = 4.0f / 3.0f;
    ambient_lighting_ = {};
    image_based_lighting_ = {};
    exponential_height_fog_ = {};
    post_process_settings_ = {};
    ssao_settings_ = {};
    image_based_lighting_resources_dirty_ = true;
    light_revision_ = 1;
    light_state_revision_ = 1;
    lights_dirty_ = true;
    ui_frame_.Clear();
}

/**
 * @brief TODO: Describe RenderSystem::Resize.
 *
 * @param window_width TODO: Describe this parameter.
 * @param window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderSystem::Resize(int window_width, int window_height)
{
    return backend_ != nullptr && backend_->Resize(window_width, window_height);
}

/**
 * @brief TODO: Describe RenderSystem::Render.
 */
void RenderSystem::Render()
{
    if (backend_ != nullptr)
    {
        backend_->Render();
    }
}

void RenderSystem::SetUiFrame(UiFrame frame)
{
    ui_frame_ = std::move(frame);
}

const UiFrame &RenderSystem::GetUiFrame() const
{
    return ui_frame_;
}

/**
 * @brief TODO: Describe RenderSystem::RegisterMeshInstance.
 *
 * @param mesh_instance TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
MeshInstanceHandle RenderSystem::RegisterMeshInstance(const MeshInstance &mesh_instance)
{
    if (mesh_instance.mesh == nullptr || mesh_instance.mesh->Empty() || mesh_instance.material == nullptr)
    {
        return {};
    }

    const Bounds local_bounds = mesh_instance.mesh->local_bounds;
    const Bounds world_bounds = TransformBounds(local_bounds, mesh_instance.transform);

    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_mesh_instances_.empty())
    {
        index = static_cast<std::uint32_t>(mesh_instances_.size());
        mesh_instances_.push_back(mesh_instance);
        mesh_instance_local_bounds_.push_back(local_bounds);
        mesh_instance_world_bounds_.push_back(world_bounds);
        mesh_instance_generations_.push_back(generation);
    }
    else
    {
        index = free_mesh_instances_.back();
        free_mesh_instances_.pop_back();
        generation = mesh_instance_generations_[index];
        mesh_instances_[index] = mesh_instance;
        mesh_instance_local_bounds_[index] = local_bounds;
        mesh_instance_world_bounds_[index] = world_bounds;
    }
    const MeshInstanceHandle handle{index, generation};

    if (backend_ != nullptr && !backend_->RegisterMeshInstance(index, mesh_instance, world_bounds))
    {
        mesh_instances_[index] = {};
        mesh_instance_local_bounds_[index] = {};
        mesh_instance_world_bounds_[index] = {};
        ++mesh_instance_generations_[index];
        if (mesh_instance_generations_[index] == 0)
        {
            ++mesh_instance_generations_[index];
        }
        free_mesh_instances_.push_back(index);
        return {};
    }

    ++mesh_instance_revision_;
    return handle;
}

/**
 * @brief TODO: Describe RenderSystem::RemoveMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 */
void RenderSystem::RemoveMeshInstance(MeshInstanceHandle handle)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }
    const std::uint32_t index = handle.Index();
    if (backend_ != nullptr)
    {
        backend_->RemoveMeshInstance(index);
    }
    mesh_instances_[index] = {};
    mesh_instance_local_bounds_[index] = {};
    mesh_instance_world_bounds_[index] = {};
    ++mesh_instance_generations_[index];
    if (mesh_instance_generations_[index] == 0)
    {
        ++mesh_instance_generations_[index];
    }
    free_mesh_instances_.push_back(index);
    ++mesh_instance_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @param flags TODO: Describe this parameter.
 */
void RenderSystem::UpdateMeshInstance(MeshInstanceHandle handle, const glm::mat4 &transform, std::uint32_t flags)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }

    const std::uint32_t index = handle.Index();
    MeshInstance &mesh_instance = mesh_instances_[index];
    if (mesh_instance.flags == flags && mesh_instance.transform == transform)
    {
        return;
    }
    mesh_instance.transform = transform;
    mesh_instance.flags = flags;
    mesh_instance_world_bounds_[index] = TransformBounds(mesh_instance_local_bounds_[index], transform);
    if (backend_ != nullptr)
    {
        backend_->UpdateMeshInstance(index, transform, mesh_instance_world_bounds_[index], flags);
    }
    ++mesh_instance_revision_;
}

bool RenderSystem::UpdateMeshSkinning(MeshInstanceHandle handle, std::span<const glm::mat4> palette)
{
    const MeshInstance *instance = ResolveMeshInstance(handle);
    if (instance == nullptr || instance->mesh == nullptr || !instance->mesh->skinned || palette.empty() ||
        palette.size() > 1024u || backend_ == nullptr)
    {
        return false;
    }
    for (const glm::mat4 &matrix : palette)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!std::isfinite(matrix[column][row]))
                {
                    return false;
                }
            }
        }
    }
    return backend_->UpdateSkinningPalette(handle.Index(), palette);
}

ParticleEmitterHandle RenderSystem::RegisterParticleEmitter(const ParticleEmitter &emitter)
{
    if (!ValidParticleEmitter(emitter))
    {
        return {};
    }

    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_particle_emitters_.empty())
    {
        index = static_cast<std::uint32_t>(particle_emitters_.size());
        particle_emitters_.emplace_back();
        particle_emitter_generations_.push_back(generation);
    }
    else
    {
        index = free_particle_emitters_.back();
        free_particle_emitters_.pop_back();
        generation = particle_emitter_generations_[index];
    }

    ParticleEmitterState &state = particle_emitters_[index];
    state = {};
    state.emitter = emitter;
    state.particles.reserve(emitter.capacity);
    state.emission_sequence = Hash(generation ^ ((index + 1u) * 0x9e3779b9u));
    ++particle_emitter_count_;
    return {index, generation};
}

void RenderSystem::RemoveParticleEmitter(ParticleEmitterHandle handle)
{
    ParticleEmitterState *state = ResolveParticleEmitterState(handle);
    if (state == nullptr)
    {
        return;
    }

    *state = {};
    const std::uint32_t index = handle.Index();
    ++particle_emitter_generations_[index];
    if (particle_emitter_generations_[index] == 0)
    {
        ++particle_emitter_generations_[index];
    }
    free_particle_emitters_.push_back(index);
    --particle_emitter_count_;
    RebuildParticleDraws();
}

void RenderSystem::UpdateParticleEmitter(ParticleEmitterHandle handle, const glm::mat4 &transform, bool emitting)
{
    ParticleEmitterState *state = ResolveParticleEmitterState(handle);
    if (state == nullptr || !FiniteTransform(transform))
    {
        return;
    }

    if (state->emitter.transform == transform && state->emitter.emitting == emitting)
    {
        return;
    }
    const bool local_space = (state->emitter.particle->flags & ParticleFlagWorldSpace) == 0;
    state->emitter.transform = transform;
    state->emitter.emitting = emitting;
    if (local_space && !state->particles.empty())
    {
        RebuildParticleDraws();
    }
}

std::uint32_t RenderSystem::EmitParticles(ParticleEmitterHandle handle, std::uint32_t count)
{
    ParticleEmitterState *state = ResolveParticleEmitterState(handle);
    if (state == nullptr || count == 0)
    {
        return 0;
    }

    const std::uint32_t available = state->emitter.capacity - static_cast<std::uint32_t>(state->particles.size());
    const std::uint32_t emitted = std::min(count, available);
    for (std::uint32_t index = 0; index < emitted; ++index)
    {
        SpawnParticle(*state, handle.Index());
    }
    if (emitted != 0)
    {
        RebuildParticleDraws();
    }
    return emitted;
}

void RenderSystem::UpdateParticles(float delta_seconds)
{
    if (!Finite(delta_seconds) || delta_seconds <= 0.0f)
    {
        return;
    }

    for (std::uint32_t emitter_index = 0; emitter_index < particle_emitters_.size(); ++emitter_index)
    {
        ParticleEmitterState &state = particle_emitters_[emitter_index];
        if (state.emitter.particle == nullptr)
        {
            continue;
        }

        const Assets::Particle &definition = *state.emitter.particle;
        const glm::vec3 gravity(definition.gravity[0], definition.gravity[1], definition.gravity[2]);
        for (SimulatedParticle &particle : state.particles)
        {
            particle.age_seconds += delta_seconds;
            if (particle.age_seconds < particle.lifetime_seconds)
            {
                particle.velocity += gravity * delta_seconds;
                particle.position += particle.velocity * delta_seconds;
            }
        }
        std::erase_if(state.particles, [](const SimulatedParticle &particle) {
            return particle.age_seconds >= particle.lifetime_seconds;
        });

        if (state.emitter.emitting && (definition.flags & ParticleFlagLoop) != 0 && definition.rate > 0.0f)
        {
            state.emission_accumulator += definition.rate * delta_seconds;
            const std::uint32_t available = state.emitter.capacity - static_cast<std::uint32_t>(state.particles.size());
            std::uint32_t emitted = available;
            if (std::isfinite(state.emission_accumulator))
            {
                const float whole_particles = std::floor(state.emission_accumulator);
                emitted = static_cast<std::uint32_t>(std::min(whole_particles, static_cast<float>(available)));
                state.emission_accumulator -= whole_particles;
            }
            else
            {
                state.emission_accumulator = 0.0f;
            }
            for (std::uint32_t index = 0; index < emitted; ++index)
            {
                SpawnParticle(state, emitter_index);
            }
        }
    }
    RebuildParticleDraws();
}

const ParticleEmitter *RenderSystem::ResolveParticleEmitter(ParticleEmitterHandle handle) const
{
    const ParticleEmitterState *state = ResolveParticleEmitterState(handle);
    return state != nullptr ? &state->emitter : nullptr;
}

std::size_t RenderSystem::GetParticleCount(ParticleEmitterHandle handle) const
{
    const ParticleEmitterState *state = ResolveParticleEmitterState(handle);
    return state != nullptr ? state->particles.size() : 0;
}

std::size_t RenderSystem::GetParticleEmitterCount() const
{
    return particle_emitter_count_;
}

std::span<const ParticleDraw> RenderSystem::GetParticleDraws() const
{
    return particle_draws_;
}

EnvironmentProbeHandle RenderSystem::RegisterEnvironmentProbe(const EnvironmentProbe &probe)
{
    if (!ValidEnvironmentProbe(probe))
    {
        return {};
    }

    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_environment_probes_.empty())
    {
        index = static_cast<std::uint32_t>(environment_probes_.size());
        environment_probes_.emplace_back();
        environment_probe_generations_.push_back(generation);
    }
    else
    {
        index = free_environment_probes_.back();
        free_environment_probes_.pop_back();
        generation = environment_probe_generations_[index];
    }
    environment_probes_[index] = probe;
    ++environment_probe_revision_;
    return {index, generation};
}

void RenderSystem::RemoveEnvironmentProbe(EnvironmentProbeHandle handle)
{
    const std::uint32_t index = handle.Index();
    if (ResolveEnvironmentProbe(handle) == nullptr)
    {
        return;
    }
    environment_probes_[index] = {};
    ++environment_probe_generations_[index];
    if (environment_probe_generations_[index] == 0)
    {
        ++environment_probe_generations_[index];
    }
    free_environment_probes_.push_back(index);
    ++environment_probe_revision_;
}

void RenderSystem::UpdateEnvironmentProbe(EnvironmentProbeHandle handle, const EnvironmentProbe &probe)
{
    const std::uint32_t index = handle.Index();
    const EnvironmentProbe *current = ResolveEnvironmentProbe(handle);
    if (current == nullptr || !ValidEnvironmentProbe(probe) || SameEnvironmentProbe(*current, probe))
    {
        return;
    }
    environment_probes_[index] = probe;
    ++environment_probe_revision_;
}

const EnvironmentProbe *RenderSystem::ResolveEnvironmentProbe(EnvironmentProbeHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < environment_probes_.size() &&
                   environment_probe_generations_[index] == handle.Generation() &&
                   environment_probes_[index].panorama != nullptr
               ? &environment_probes_[index]
               : nullptr;
}

const std::vector<EnvironmentProbe> &RenderSystem::GetEnvironmentProbes() const
{
    return environment_probes_;
}

std::uint64_t RenderSystem::GetEnvironmentProbeRevision() const
{
    return environment_probe_revision_;
}

RenderSystem::ParticleEmitterState *RenderSystem::ResolveParticleEmitterState(ParticleEmitterHandle handle)
{
    const std::uint32_t index = handle.Index();
    return handle && index < particle_emitters_.size() && particle_emitter_generations_[index] == handle.Generation()
               ? &particle_emitters_[index]
               : nullptr;
}

const RenderSystem::ParticleEmitterState *RenderSystem::ResolveParticleEmitterState(ParticleEmitterHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < particle_emitters_.size() && particle_emitter_generations_[index] == handle.Generation()
               ? &particle_emitters_[index]
               : nullptr;
}

void RenderSystem::SpawnParticle(ParticleEmitterState &state, std::uint32_t emitter_index)
{
    const Assets::Particle &definition = *state.emitter.particle;
    std::uint32_t random = Hash(state.emission_sequence++ ^ ((emitter_index + 1u) * 0x85ebca6bu));
    const float lifetime = glm::mix(definition.lifetime[0], definition.lifetime[1], RandomUnit(random));
    const float speed = glm::mix(definition.speed[0], definition.speed[1], RandomUnit(random));
    const float azimuth = RandomUnit(random) * (2.0f * std::numbers::pi_v<float>);
    const float cos_spread = std::cos(definition.spread);
    const float cos_angle = glm::mix(1.0f, cos_spread, RandomUnit(random));
    const float sin_angle = std::sqrt(std::max(1.0f - (cos_angle * cos_angle), 0.0f));
    glm::vec3 direction(cos_angle, std::cos(azimuth) * sin_angle, std::sin(azimuth) * sin_angle);

    SimulatedParticle particle;
    particle.lifetime_seconds = lifetime;
    if ((definition.flags & ParticleFlagWorldSpace) != 0)
    {
        particle.position = glm::vec3(state.emitter.transform[3]);
        direction = glm::mat3(state.emitter.transform) * direction;
        const float direction_length = glm::length(direction);
        direction = direction_length > 0.00001f ? direction / direction_length : glm::vec3(1.0f, 0.0f, 0.0f);
    }
    particle.velocity = direction * speed;
    state.particles.push_back(particle);
}

void RenderSystem::RebuildParticleDraws()
{
    std::size_t particle_count = 0;
    for (const ParticleEmitterState &state : particle_emitters_)
    {
        particle_count += state.particles.size();
    }
    particle_draws_.clear();
    particle_draws_.reserve(particle_count);

    for (const ParticleEmitterState &state : particle_emitters_)
    {
        if (state.emitter.particle == nullptr)
        {
            continue;
        }
        const Assets::Particle &definition = *state.emitter.particle;
        const glm::vec4 start_color(definition.start_color[0], definition.start_color[1], definition.start_color[2],
                                    definition.start_color[3]);
        const glm::vec4 end_color(definition.end_color[0], definition.end_color[1], definition.end_color[2],
                                  definition.end_color[3]);
        const bool world_space = (definition.flags & ParticleFlagWorldSpace) != 0;
        for (const SimulatedParticle &particle : state.particles)
        {
            const float progress = particle.lifetime_seconds > 0.0f
                                       ? glm::clamp(particle.age_seconds / particle.lifetime_seconds, 0.0f, 1.0f)
                                       : 1.0f;
            ParticleDraw draw;
            draw.position = world_space ? particle.position
                                        : glm::vec3(state.emitter.transform * glm::vec4(particle.position, 1.0f));
            draw.size = glm::mix(definition.size[0], definition.size[1], progress);
            draw.color = glm::mix(start_color, end_color, progress);
            draw.texture = state.emitter.texture.get();
            draw.blend_mode = static_cast<ParticleBlendMode>(definition.blend_mode);
            particle_draws_.push_back(draw);
        }
    }
}

/**
 * @brief TODO: Describe RenderSystem::RegisterLight.
 *
 * @param light TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
LightHandle RenderSystem::RegisterLight(const Light &light)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_lights_.empty())
    {
        index = static_cast<std::uint32_t>(direct_lights_.size());
        direct_lights_.push_back(light);
        light_shadow_handles_.emplace_back();
        light_generations_.push_back(generation);
    }
    else
    {
        index = free_lights_.back();
        free_lights_.pop_back();
        generation = light_generations_[index];
        direct_lights_[index] = light;
        light_shadow_handles_[index] = {};
    }
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
    return {index, generation};
}

/**
 * @brief TODO: Describe RenderSystem::RemoveLight.
 *
 * @param id TODO: Describe this parameter.
 */
void RenderSystem::RemoveLight(LightHandle id)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    const std::uint32_t index = id.Index();
    direct_lights_[index].enabled = false;
    light_shadow_handles_[index] = {};
    ++light_generations_[index];
    if (light_generations_[index] == 0)
    {
        ++light_generations_[index];
    }
    free_lights_.push_back(index);
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateLight.
 *
 * @param id TODO: Describe this parameter.
 * @param light TODO: Describe this parameter.
 */
void RenderSystem::UpdateLight(LightHandle id, const Light &light)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    const std::uint32_t index = id.Index();
    if (SameLight(direct_lights_[index], light))
    {
        return;
    }
    direct_lights_[index] = light;
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetMeshInstances.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<MeshInstance> &RenderSystem::GetMeshInstances() const
{
    return mesh_instances_;
}

/**
 * @brief TODO: Describe RenderSystem::GetMeshInstanceRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetMeshInstanceRevision() const
{
    return mesh_instance_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetDirectLights.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<Light> &RenderSystem::GetDirectLights() const
{
    return direct_lights_;
}

/**
 * @brief TODO: Describe RenderSystem::ResolveMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const MeshInstance *RenderSystem::ResolveMeshInstance(MeshInstanceHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < mesh_instances_.size() && mesh_instance_generations_[index] == handle.Generation()
               ? &mesh_instances_[index]
               : nullptr;
}

/**
 * @brief TODO: Describe RenderSystem::ResolveLight.
 *
 * @param handle TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const Light *RenderSystem::ResolveLight(LightHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < direct_lights_.size() && light_generations_[index] == handle.Generation()
               ? &direct_lights_[index]
               : nullptr;
}

/**
 * @brief TODO: Describe RenderSystem::GetGpuLights.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<GpuLight> &RenderSystem::GetGpuLights()
{
    if (lights_dirty_)
    {
        RebuildGpuLights();
    }
    return gpu_lights_;
}

/**
 * @brief TODO: Describe RenderSystem::GetLightRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetLightRevision() const
{
    return light_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetLightStateRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetLightStateRevision() const
{
    return light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::SetLightShadowHandles.
 *
 * @param handles TODO: Describe this parameter.
 */
void RenderSystem::SetLightShadowHandles(const std::vector<LightShadowBinding> &handles)
{
    if (light_shadow_handles_ == handles)
    {
        return;
    }

    light_shadow_handles_ = handles;
    if (light_shadow_handles_.size() < direct_lights_.size())
    {
        light_shadow_handles_.resize(direct_lights_.size());
    }

    lights_dirty_ = true;
    ++light_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetCameraFrameData.
 *
 * @return TODO: Describe the return value.
 */
const CameraFrameData &RenderSystem::GetCameraFrameData() const
{
    return camera_frame_data_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateCamera.
 *
 * @param camera TODO: Describe this parameter.
 */
void RenderSystem::UpdateCamera(const Camera &camera)
{
    active_camera_ = camera;
    camera_frame_data_ = active_camera_.BuildFrameData(camera_aspect_ratio_);
}

/**
 * @brief TODO: Describe RenderSystem::SetCameraAspectRatio.
 *
 * @param aspect_ratio TODO: Describe this parameter.
 */
void RenderSystem::SetCameraAspectRatio(float aspect_ratio)
{
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f)
    {
        return;
    }
    camera_aspect_ratio_ = aspect_ratio;
    camera_frame_data_ = active_camera_.BuildFrameData(camera_aspect_ratio_);
}

/**
 * @brief TODO: Describe RenderSystem::ActiveCamera.
 *
 * @return TODO: Describe the return value.
 */
const Camera &RenderSystem::ActiveCamera() const
{
    return active_camera_;
}

/**
 * @brief TODO: Describe RenderSystem::SetAmbientLighting.
 *
 * @param ambient TODO: Describe this parameter.
 */
void RenderSystem::SetAmbientLighting(const AmbientLighting &ambient)
{
    ambient_lighting_ = ambient;
}

/**
 * @brief TODO: Describe RenderSystem::GetAmbientLighting.
 *
 * @return TODO: Describe the return value.
 */
const AmbientLighting &RenderSystem::GetAmbientLighting() const
{
    return ambient_lighting_;
}

/**
 * @brief TODO: Describe RenderSystem::SetImageBasedLighting.
 *
 * @param lighting TODO: Describe this parameter.
 */
void RenderSystem::SetImageBasedLighting(const ImageBasedLighting &lighting)
{
    const bool resources_changed =
        image_based_lighting_.enabled != lighting.enabled || image_based_lighting_.panorama != lighting.panorama;
    image_based_lighting_ = lighting;
    image_based_lighting_resources_dirty_ = image_based_lighting_resources_dirty_ || resources_changed;
}

/**
 * @brief TODO: Describe RenderSystem::GetImageBasedLighting.
 *
 * @return TODO: Describe the return value.
 */
const ImageBasedLighting &RenderSystem::GetImageBasedLighting() const
{
    return image_based_lighting_;
}

/**
 * @brief TODO: Describe RenderSystem::SetExponentialHeightFog.
 *
 * @param fog TODO: Describe this parameter.
 */
void RenderSystem::SetExponentialHeightFog(const ExponentialHeightFog &fog)
{
    exponential_height_fog_ = fog;
}

/**
 * @brief TODO: Describe RenderSystem::GetExponentialHeightFog.
 *
 * @return TODO: Describe the return value.
 */
const ExponentialHeightFog &RenderSystem::GetExponentialHeightFog() const
{
    return exponential_height_fog_;
}

/**
 * @brief TODO: Describe RenderSystem::SetPostProcessSettings.
 *
 * @param settings TODO: Describe this parameter.
 */
void RenderSystem::SetPostProcessSettings(const PostProcessSettings &settings)
{
    post_process_settings_ = settings;
}

/**
 * @brief TODO: Describe RenderSystem::GetPostProcessSettings.
 *
 * @return TODO: Describe the return value.
 */
const PostProcessSettings &RenderSystem::GetPostProcessSettings() const
{
    return post_process_settings_;
}

/**
 * @brief TODO: Describe RenderSystem::SetSSAOSettings.
 *
 * @param settings TODO: Describe this parameter.
 */
void RenderSystem::SetSSAOSettings(const SSAOSettings &settings)
{
    ssao_settings_ = settings;
}

/**
 * @brief TODO: Describe RenderSystem::GetSSAOSettings.
 *
 * @return TODO: Describe the return value.
 */
const SSAOSettings &RenderSystem::GetSSAOSettings() const
{
    return ssao_settings_;
}

/**
 * @brief TODO: Describe RenderSystem::ConsumeImageBasedLightingResourcesDirty.
 *
 * @return TODO: Describe the return value.
 */
bool RenderSystem::ConsumeImageBasedLightingResourcesDirty()
{
    const bool result = image_based_lighting_resources_dirty_;
    image_based_lighting_resources_dirty_ = false;
    return result;
}

/**
 * @brief TODO: Describe RenderSystem::RebuildGpuLights.
 */
void RenderSystem::RebuildGpuLights()
{
    gpu_lights_.clear();

    for (size_t light_index = 0; light_index < direct_lights_.size(); ++light_index)
    {
        const Light &light = direct_lights_[light_index];
        if (!light.enabled)
        {
            continue;
        }
        if (gpu_lights_.size() >= MaxGpuLights)
        {
            break;
        }

        GpuLight gpu_light{};
        gpu_light.position_range = glm::vec4(light.position, light.range);
        gpu_light.direction_spot = glm::vec4(light.direction, light.spot_outer_cos);
        gpu_light.color_intensity = glm::vec4(light.color, light.intensity);
        LightShadowBinding shadow_handle;
        if (light_index < light_shadow_handles_.size())
        {
            shadow_handle = light_shadow_handles_[light_index];
        }
        gpu_light.params = glm::vec4(static_cast<float>(light.type), light.spot_inner_cos,
                                     static_cast<float>(shadow_handle.index), static_cast<float>(shadow_handle.type));
        gpu_lights_.push_back(gpu_light);
    }

    lights_dirty_ = false;
}

} // namespace CEngine::Renderer
