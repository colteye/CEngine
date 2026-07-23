#ifndef CENGINE_RENDERER_OPENGL_RENDER_BACKEND_H
#define CENGINE_RENDERER_OPENGL_RENDER_BACKEND_H

#include "renderer/material.h"
#include "renderer/mesh_instance.h"
#include "renderer/opengl/render_data.h"
#include "renderer/opengl/shaders/deferred_lighting.h"
#include "renderer/opengl/shaders/fullscreen_blit.h"
#include "renderer/opengl/shaders/pbr_geometry_pass.h"
#include "renderer/opengl/shaders/pbr_standard.h"
#include "renderer/opengl/shaders/shader.h"
#include "renderer/opengl/shaders/ssao.h"
#include "renderer/opengl/shadow_system.h"
#include "renderer/render_backend.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace CEngine::Renderer::OpenGL
{

struct MaterialResources
{
    GLuint albedo_tex = 0;
    GLuint normal_tex = 0;
    GLuint metallic_roughness_ao_tex = 0;
    bool owns_albedo_tex = false;
    bool owns_normal_tex = false;
    bool owns_metallic_roughness_ao_tex = false;
    std::size_t references = 0;

    void Destroy();
};

struct LightmapResources
{
    GLuint texture = 0;
    std::size_t references = 0;
};

struct FrameResources
{
    GLuint g_buffer_fbo = 0;
    GLuint g_albedo = 0;
    GLuint g_normal_roughness = 0;
    GLuint g_material = 0;
    GLuint g_baked_light = 0;
    GLuint scene_depth = 0;
    GLuint opaque_lit_fbo = 0;
    GLuint opaque_lit_color = 0;
    GLuint ssao_fbo = 0;
    GLuint ssao = 0;
    GLuint scene_color_fbo = 0;
    GLuint scene_color = 0;

    void Destroy();
};

struct ShaderPasses
{
    std::unique_ptr<SSAO> ssao;
    std::unique_ptr<PBRStandard> pbr_forward;
    std::unique_ptr<PBRGeometryPass> pbr_geometry;
    std::unique_ptr<DeferredLighting> deferred_lighting;
    std::unique_ptr<FullscreenBlit> fullscreen_blit;
};

struct EnvironmentResources
{
    GLuint panorama = 0;
    GLuint environment_map = 0;
    GLuint irradiance_map = 0;
    GLuint prefiltered_map = 0;
    GLuint capture_fbo = 0;
    GLuint capture_rbo = 0;
    GLuint cube_vao = 0;
    GLuint cube_vbo = 0;
    std::unique_ptr<ShaderProgram> panorama_to_cube;
    std::unique_ptr<ShaderProgram> irradiance;
    std::unique_ptr<ShaderProgram> prefilter;
    std::unique_ptr<ShaderProgram> skybox;
    GLint skybox_view = -1;
    GLint skybox_projection = -1;
    GLint skybox_intensity = -1;
    GLint skybox_rotation = -1;
    GLint skybox_environment_map = -1;
    void Destroy();
};

struct MeshResources
{
    MeshResources() = default;
    MeshResources(const MeshResources &) = delete;
    MeshResources &operator=(const MeshResources &) = delete;
    MeshResources(MeshResources &&other) noexcept;
    MeshResources &operator=(MeshResources &&other) noexcept;
    ~MeshResources();

    void Destroy();

    GLuint vertex_array_obj = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;
    GLsizei index_count = 0;
    std::size_t references = 0;
};

class RenderBackend final : public IRenderBackend
{
  public:
    bool Initialize(RenderSystem &rendering, GLFWwindow *window, int window_width, int window_height) override;
    void Shutdown() override;
    bool Resize(int window_width, int window_height) override;
    void Render() override;
    bool RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                              const Bounds &world_bounds) override;
    void RemoveMeshInstance(std::uint32_t slot) override;
    void UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                            std::uint32_t flags) override;

  private:
    static MeshResources UploadMesh(const Mesh &mesh);
    bool RegisterMaterial(const Material *material);
    void RemoveMaterial(const Material *material);
    bool RegisterLightmap(const Texture *lightmap);
    void RemoveLightmap(const Texture *lightmap);

    bool CreateFrameResources(int width, int height);
    void DestroyFrameResources();
    void BuildRenderQueues();
    void RenderGeometryPass();
    void RenderDeferredLightingPass();
    void RenderAmbientOcclusionPass();
    void SyncEnvironmentResources();
    bool BuildEnvironmentResources();
    void RenderSkybox();
    void DrawEnvironmentCube() const;
    void RenderForwardQueue(const std::vector<uint32_t> &queue, bool transparent);
    void PresentSceneColor();
    static void Draw(const DrawItem &item);
    [[nodiscard]] static RenderQueue ClassifyRenderMode(MaterialRenderMode mode);
    [[nodiscard]] static bool DrawsShadowCaster(const DrawItem &item);
    void RenderScreenSpaceQuad();
    PBRStandard *GetShader(MaterialShaderType shader_type);

    GLuint quad_VAO_ = 0;
    GLuint quad_VBO_ = 0;
    GLuint default_white_texture_ = 0;
    GLuint default_normal_texture_ = 0;

    RenderSystem *rendering_ = nullptr;
    int window_width_ = 0;
    int window_height_ = 0;

    FrameResources frame_resources_;
    ShadowSystem shadow_system_;
    RenderQueues render_queues_;
    ShaderPasses shader_passes_;
    EnvironmentResources environment_resources_;
    std::unordered_map<const Mesh *, MeshResources> mesh_resources_;
    std::unordered_map<const Material *, MaterialResources> material_resources_;
    std::unordered_map<const Texture *, LightmapResources> lightmap_resources_;
    std::vector<DrawItem> draw_items_;
    std::vector<float> camera_distance_squared_;
};

} // namespace CEngine::Renderer::OpenGL
#endif
