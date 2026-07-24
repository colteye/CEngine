#include "renderer/opengl/particle_renderer.h"

#include "renderer/opengl/texture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{
namespace
{

struct SortedParticle
{
    const ParticleDraw *particle = nullptr;
    float distance_squared = 0.0f;
};

} // namespace

ParticleRenderer::~ParticleRenderer()
{
    Shutdown();
}

bool ParticleRenderer::Initialize()
{
    Shutdown();
    shader_ = std::make_unique<ShaderProgram>();
    if (!shader_->Load("shaders/opengl/particle.vert", "shaders/opengl/particle.frag"))
    {
        shader_.reset();
        return false;
    }

    constexpr std::array<glm::vec4, 6> Quad = {
        glm::vec4(-0.5f, -0.5f, 0.0f, 0.0f), glm::vec4(0.5f, -0.5f, 1.0f, 0.0f), glm::vec4(0.5f, 0.5f, 1.0f, 1.0f),
        glm::vec4(-0.5f, -0.5f, 0.0f, 0.0f), glm::vec4(0.5f, 0.5f, 1.0f, 1.0f),  glm::vec4(-0.5f, 0.5f, 0.0f, 1.0f),
    };

    glGenVertexArrays(1, &vertex_array_);
    glGenBuffers(1, &quad_buffer_);
    glGenBuffers(1, &instance_buffer_);
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, quad_buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Quad), Quad.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), reinterpret_cast<void *>(sizeof(glm::vec2)));
    BindInstanceAttributes(0);
    glBindVertexArray(0);

    white_texture_ = TextureLoader::CreateSolid(255, 255, 255);
    if (white_texture_ == 0)
    {
        Shutdown();
        return false;
    }

    const GLuint program = shader_->GetId();
    view_projection_location_ = glGetUniformLocation(program, "view_projection");
    camera_right_location_ = glGetUniformLocation(program, "camera_right");
    camera_up_location_ = glGetUniformLocation(program, "camera_up");
    use_texture_location_ = glGetUniformLocation(program, "use_texture");
    blend_mode_location_ = glGetUniformLocation(program, "blend_mode");
    shader_->Use();
    glUniform1i(glGetUniformLocation(program, "particle_texture"), 0);
    glUseProgram(0);
    return true;
}

void ParticleRenderer::Shutdown()
{
    for (const auto &[texture, resource] : textures_)
    {
        static_cast<void>(texture);
        glDeleteTextures(1, &resource.id);
    }
    textures_.clear();
    shader_.reset();
    if (white_texture_ != 0)
    {
        glDeleteTextures(1, &white_texture_);
    }
    if (instance_buffer_ != 0)
    {
        glDeleteBuffers(1, &instance_buffer_);
    }
    if (quad_buffer_ != 0)
    {
        glDeleteBuffers(1, &quad_buffer_);
    }
    if (vertex_array_ != 0)
    {
        glDeleteVertexArrays(1, &vertex_array_);
    }
    white_texture_ = 0;
    instance_buffer_ = 0;
    quad_buffer_ = 0;
    vertex_array_ = 0;
    view_projection_location_ = -1;
    camera_right_location_ = -1;
    camera_up_location_ = -1;
    use_texture_location_ = -1;
    blend_mode_location_ = -1;
    frame_ = 0;
    instances_.clear();
    batches_.clear();
}

void ParticleRenderer::Render(std::span<const ParticleDraw> particles, const CameraFrameData &camera,
                              GLuint framebuffer, int width, int height)
{
    ++frame_;
    BuildBatches(particles, camera.camera_position);
    PruneTextures();
    if (instances_.empty() || shader_ == nullptr || vertex_array_ == 0 || width <= 0 || height <= 0)
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    shader_->Use();
    const glm::mat4 inverse_view = glm::inverse(camera.view);
    const glm::mat4 view_projection = camera.proj * camera.view;
    glUniformMatrix4fv(view_projection_location_, 1, GL_FALSE, glm::value_ptr(view_projection));
    glUniform3fv(camera_right_location_, 1, glm::value_ptr(glm::vec3(inverse_view[0])));
    glUniform3fv(camera_up_location_, 1, glm::value_ptr(glm::vec3(inverse_view[1])));

    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, instance_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances_.size() * sizeof(GpuInstance)), instances_.data(),
                 GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    for (const DrawBatch &batch : batches_)
    {
        const GLuint texture = ResolveTexture(batch.texture);
        glBindTexture(GL_TEXTURE_2D, texture != 0 ? texture : white_texture_);
        glUniform1i(use_texture_location_, texture != 0 ? 1 : 0);
        glUniform1i(blend_mode_location_, static_cast<GLint>(batch.blend_mode));
        switch (batch.blend_mode)
        {
        case ParticleBlendMode::Alpha:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case ParticleBlendMode::Additive:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case ParticleBlendMode::PremultipliedAlpha:
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        }
        BindInstanceAttributes(batch.first);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(batch.count));
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

GLuint ParticleRenderer::ResolveTexture(const Texture *texture)
{
    if (texture == nullptr || texture->Empty())
    {
        return 0;
    }
    if (auto found = textures_.find(texture); found != textures_.end())
    {
        found->second.last_used_frame = frame_;
        return found->second.id;
    }

    TextureResource resource;
    resource.id = TextureLoader::Load(*texture, TextureColorSpace::Srgb);
    resource.last_used_frame = frame_;
    if (resource.id == 0)
    {
        return 0;
    }
    textures_.emplace(texture, resource);
    return resource.id;
}

void ParticleRenderer::PruneTextures()
{
    for (auto resource = textures_.begin(); resource != textures_.end();)
    {
        if (resource->second.last_used_frame + 1u < frame_)
        {
            glDeleteTextures(1, &resource->second.id);
            resource = textures_.erase(resource);
        }
        else
        {
            ++resource;
        }
    }
}

void ParticleRenderer::BuildBatches(std::span<const ParticleDraw> particles, const glm::vec3 &camera_position)
{
    std::vector<SortedParticle> sorted;
    sorted.reserve(particles.size());
    for (const ParticleDraw &particle : particles)
    {
        if (particle.size <= 0.0f || particle.color.a <= 0.0f)
        {
            continue;
        }
        const glm::vec3 offset = particle.position - camera_position;
        sorted.push_back({&particle, glm::dot(offset, offset)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const SortedParticle &left, const SortedParticle &right) {
        if (left.particle->blend_mode != right.particle->blend_mode)
        {
            return left.particle->blend_mode < right.particle->blend_mode;
        }
        if (left.particle->blend_mode != ParticleBlendMode::Additive && left.distance_squared != right.distance_squared)
        {
            return left.distance_squared > right.distance_squared;
        }
        return std::less<const Texture *>{}(left.particle->texture, right.particle->texture);
    });

    instances_.clear();
    batches_.clear();
    instances_.reserve(sorted.size());
    batches_.reserve(sorted.size());
    for (const SortedParticle &item : sorted)
    {
        const ParticleDraw &particle = *item.particle;
        if (batches_.empty() || batches_.back().texture != particle.texture ||
            batches_.back().blend_mode != particle.blend_mode)
        {
            batches_.push_back({
                .first = instances_.size(),
                .texture = particle.texture,
                .blend_mode = particle.blend_mode,
            });
        }
        instances_.push_back({
            .position = particle.position,
            .size = particle.size,
            .color = particle.color,
        });
        ++batches_.back().count;
    }
}

void ParticleRenderer::BindInstanceAttributes(std::size_t first) const
{
    glBindBuffer(GL_ARRAY_BUFFER, instance_buffer_);
    const std::size_t base = first * sizeof(GpuInstance);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuInstance),
                          reinterpret_cast<void *>(base + offsetof(GpuInstance, position)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GpuInstance),
                          reinterpret_cast<void *>(base + offsetof(GpuInstance, size)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GpuInstance),
                          reinterpret_cast<void *>(base + offsetof(GpuInstance, color)));
    glVertexAttribDivisor(4, 1);
}

} // namespace CEngine::Renderer::OpenGL
