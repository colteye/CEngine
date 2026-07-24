#ifndef CENGINE_RENDERER_OPENGL_PARTICLE_RENDERER_H
#define CENGINE_RENDERER_OPENGL_PARTICLE_RENDERER_H

#include "renderer/camera.h"
#include "renderer/opengl/shaders/shader.h"
#include "renderer/particle_emitter.h"

#include <glad/glad.h>

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace CEngine::Renderer::OpenGL
{

class ParticleRenderer
{
  public:
    ParticleRenderer() = default;
    ~ParticleRenderer();
    ParticleRenderer(const ParticleRenderer &) = delete;
    ParticleRenderer &operator=(const ParticleRenderer &) = delete;
    ParticleRenderer(ParticleRenderer &&) = delete;
    ParticleRenderer &operator=(ParticleRenderer &&) = delete;

    bool Initialize();
    void Shutdown();
    void Render(std::span<const ParticleDraw> particles, const CameraFrameData &camera, GLuint framebuffer, int width,
                int height);

  private:
    struct GpuInstance
    {
        glm::vec3 position = glm::vec3(0.0f);
        float size = 0.0f;
        glm::vec4 color = glm::vec4(1.0f);
    };

    struct DrawBatch
    {
        std::size_t first = 0;
        std::size_t count = 0;
        const Texture *texture = nullptr;
        ParticleBlendMode blend_mode = ParticleBlendMode::Alpha;
    };

    struct TextureResource
    {
        GLuint id = 0;
        std::uint64_t last_used_frame = 0;
    };

    [[nodiscard]] GLuint ResolveTexture(const Texture *texture);
    void PruneTextures();
    void BuildBatches(std::span<const ParticleDraw> particles, const glm::vec3 &camera_position);
    void BindInstanceAttributes(std::size_t first) const;

    std::unique_ptr<ShaderProgram> shader_;
    GLuint vertex_array_ = 0;
    GLuint quad_buffer_ = 0;
    GLuint instance_buffer_ = 0;
    GLuint white_texture_ = 0;
    GLint view_projection_location_ = -1;
    GLint camera_right_location_ = -1;
    GLint camera_up_location_ = -1;
    GLint use_texture_location_ = -1;
    GLint blend_mode_location_ = -1;
    std::uint64_t frame_ = 0;
    std::unordered_map<const Texture *, TextureResource> textures_;
    std::vector<GpuInstance> instances_;
    std::vector<DrawBatch> batches_;
};

} // namespace CEngine::Renderer::OpenGL

#endif
