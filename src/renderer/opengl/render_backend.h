//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/render_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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

/**
 * @brief TODO: Describe MaterialResources.
 */
struct MaterialResources
{
    GLuint albedo_tex = 0;
    GLuint normal_tex = 0;
    GLuint metallic_roughness_ao_tex = 0;
    bool owns_albedo_tex = false;
    bool owns_normal_tex = false;
    bool owns_metallic_roughness_ao_tex = false;
    std::size_t references = 0;

    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();
};

/**
 * @brief TODO: Describe LightmapResources.
 */
struct LightmapResources
{
    GLuint texture = 0;
    std::size_t references = 0;
};

/**
 * @brief TODO: Describe FrameResources.
 */
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

    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();
};

/**
 * @brief TODO: Describe ShaderPasses.
 */
struct ShaderPasses
{
    std::unique_ptr<SSAO> ssao;
    std::unique_ptr<PBRStandard> pbr_forward;
    std::unique_ptr<PBRGeometryPass> pbr_geometry;
    std::unique_ptr<DeferredLighting> deferred_lighting;
    std::unique_ptr<FullscreenBlit> fullscreen_blit;
};

/**
 * @brief TODO: Describe EnvironmentResources.
 */
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
    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();
};

/**
 * @brief TODO: Describe MeshResources.
 */
struct MeshResources
{
    /**
     * @brief TODO: Describe MeshResources.
     */
    MeshResources() = default;
    /**
     * @brief TODO: Describe MeshResources.
     */
    MeshResources(const MeshResources &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    MeshResources &operator=(const MeshResources &) = delete;
    /**
     * @brief TODO: Describe MeshResources.
     *
     * @param other TODO: Describe this parameter.
     */
    MeshResources(MeshResources &&other) noexcept;
    /**
     * @brief TODO: Describe operator=.
     *
     * @param other TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    MeshResources &operator=(MeshResources &&other) noexcept;
    /**
     * @brief TODO: Describe ~MeshResources.
     */
    ~MeshResources();

    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();

    GLuint vertex_array_obj = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;
    GLsizei index_count = 0;
    std::size_t references = 0;
};

/**
 * @brief TODO: Describe RenderBackend.
 */
class RenderBackend final : public IRenderBackend
{
  public:
    /**
     * @brief TODO: Describe Initialize.
     *
     * @param rendering TODO: Describe this parameter.
     * @param window TODO: Describe this parameter.
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Initialize(RenderSystem &rendering, GLFWwindow *window, int window_width, int window_height) override;
    /**
     * @brief TODO: Describe Shutdown.
     */
    void Shutdown() override;
    /**
     * @brief TODO: Describe Resize.
     *
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Resize(int window_width, int window_height) override;
    /**
     * @brief TODO: Describe Render.
     */
    void Render() override;
    /**
     * @brief TODO: Describe RegisterMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param mesh_instance TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                              const Bounds &world_bounds) override;
    /**
     * @brief TODO: Describe RemoveMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     */
    void RemoveMeshInstance(std::uint32_t slot) override;
    /**
     * @brief TODO: Describe UpdateMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param transform TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @param flags TODO: Describe this parameter.
     */
    void UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                            std::uint32_t flags) override;

  private:
    /**
     * @brief TODO: Describe UploadMesh.
     *
     * @param mesh TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    static MeshResources UploadMesh(const Mesh &mesh);
    /**
     * @brief TODO: Describe RegisterMaterial.
     *
     * @param material TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool RegisterMaterial(const Material *material);
    /**
     * @brief TODO: Describe RemoveMaterial.
     *
     * @param material TODO: Describe this parameter.
     */
    void RemoveMaterial(const Material *material);
    /**
     * @brief TODO: Describe RegisterLightmap.
     *
     * @param lightmap TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool RegisterLightmap(const Texture *lightmap);
    /**
     * @brief TODO: Describe RemoveLightmap.
     *
     * @param lightmap TODO: Describe this parameter.
     */
    void RemoveLightmap(const Texture *lightmap);

    /**
     * @brief TODO: Describe CreateFrameResources.
     *
     * @param width TODO: Describe this parameter.
     * @param height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool CreateFrameResources(int width, int height);
    /**
     * @brief TODO: Describe DestroyFrameResources.
     */
    void DestroyFrameResources();
    /**
     * @brief TODO: Describe BuildRenderQueues.
     */
    void BuildRenderQueues();
    /**
     * @brief TODO: Describe RenderGeometryPass.
     */
    void RenderGeometryPass();
    /**
     * @brief TODO: Describe RenderDeferredLightingPass.
     */
    void RenderDeferredLightingPass();
    /**
     * @brief TODO: Describe RenderAmbientOcclusionPass.
     */
    void RenderAmbientOcclusionPass();
    /**
     * @brief TODO: Describe SyncEnvironmentResources.
     */
    void SyncEnvironmentResources();
    /**
     * @brief TODO: Describe BuildEnvironmentResources.
     *
     * @return TODO: Describe the return value.
     */
    bool BuildEnvironmentResources();
    /**
     * @brief TODO: Describe RenderSkybox.
     */
    void RenderSkybox();
    /**
     * @brief TODO: Describe DrawEnvironmentCube.
     */
    void DrawEnvironmentCube() const;
    /**
     * @brief TODO: Describe RenderForwardQueue.
     *
     * @param queue TODO: Describe this parameter.
     * @param transparent TODO: Describe this parameter.
     */
    void RenderForwardQueue(const std::vector<uint32_t> &queue, bool transparent);
    /**
     * @brief TODO: Describe PresentSceneColor.
     */
    void PresentSceneColor();
    /**
     * @brief TODO: Describe Draw.
     *
     * @param item TODO: Describe this parameter.
     */
    static void Draw(const DrawItem &item);
    /**
     * @brief TODO: Describe ClassifyRenderMode.
     *
     * @param mode TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] static RenderQueue ClassifyRenderMode(MaterialRenderMode mode);
    /**
     * @brief TODO: Describe DrawsShadowCaster.
     *
     * @param item TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] static bool DrawsShadowCaster(const DrawItem &item);
    /**
     * @brief TODO: Describe RenderScreenSpaceQuad.
     */
    void RenderScreenSpaceQuad();
    /**
     * @brief TODO: Describe GetShader.
     *
     * @param shader_type TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
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
