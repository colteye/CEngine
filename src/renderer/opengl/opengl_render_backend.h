#ifndef OPENGL_RENDER_BACKEND_H
#define OPENGL_RENDER_BACKEND_H

#include "renderer/material.h"
#include "renderer/render_backend.h"
#include "renderer/renderable.h"
#include "renderer/opengl/opengl_render_data.h"
#include "renderer/opengl/opengl_shadow_system.h"
#include "renderer/opengl/shaders/deferred_lighting.h"
#include "renderer/opengl/shaders/fullscreen_blit.h"
#include "renderer/opengl/shaders/pbr_geometry_pass.h"
#include "renderer/opengl/shaders/pbr_standard.h"
#include "renderer/opengl/shaders/ssao.h"
#include "renderer/opengl/shaders/shader.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace CEngine::Renderer {

struct OpenGLMaterialResources {
	GLuint albedo_tex = 0;
	GLuint normal_tex = 0;
	GLuint metallic_roughness_ao_tex = 0;
	bool owns_albedo_tex = false;
	bool owns_normal_tex = false;
	bool owns_metallic_roughness_ao_tex = false;

	void Destroy();
};

struct OpenGLLightmapResources {
	GLuint texture = 0;
	std::size_t references = 0;
};

struct OpenGLFrameResources {
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

struct OpenGLShaderPasses {
	std::unique_ptr<SSAO> ssao;
	std::unique_ptr<PBRStandard> pbr_forward;
	std::unique_ptr<PBRGeometryPass> pbr_geometry;
	std::unique_ptr<DeferredLighting> deferred_lighting;
	std::unique_ptr<FullscreenBlit> fullscreen_blit;
};

struct OpenGLEnvironmentResources {
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

struct OpenGLMeshResources {
	OpenGLMeshResources() = default;
	OpenGLMeshResources(const OpenGLMeshResources&) = delete;
	OpenGLMeshResources& operator=(const OpenGLMeshResources&) = delete;
	OpenGLMeshResources(OpenGLMeshResources&& other) noexcept;
	OpenGLMeshResources& operator=(OpenGLMeshResources&& other) noexcept;
	~OpenGLMeshResources();

	void Destroy();

	GLuint vertex_array_obj = 0;
	GLuint vertex_buffer = 0;
	GLuint uv_buffer = 0;
	GLuint lightmap_uv_buffer = 0;
	GLuint normal_buffer = 0;
	GLuint tangent_buffer = 0;
	GLsizei vertex_count = 0;
	std::size_t references = 0;
};

class OpenGLRenderBackend final : public IRenderBackend
{
public:
	bool Initialize(RenderSystem& rendering,
		GLFWwindow* window, int window_width, int window_height) override;
	void Shutdown() override;
	bool Resize(int window_width, int window_height) override;
	void Render() override;
	bool RegisterRenderable(std::uint32_t slot, const Renderable& renderable) override;
	void RemoveRenderable(std::uint32_t slot) override;
	void UpdateRenderable(std::uint32_t slot, const glm::mat4& transform,
		const Bounds& world_bounds, std::uint32_t flags) override;
	bool RegisterMaterial(Material* material) override;
	void RemoveMaterial(Material* material) override;
	bool RegisterLightmap(const Texture* lightmap) override;
	void RemoveLightmap(const Texture* lightmap) override;

private:
	static OpenGLMeshResources UploadMesh(const MeshData& mesh);

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
	void RenderForwardQueue(const std::vector<uint32_t>& queue, bool transparent);
	void PresentSceneColor();
	void DrawItem(const OpenGLDrawItem& item) const;
	OpenGLRenderQueue ClassifyRenderMode(MaterialRenderMode mode) const;
	bool DrawsShadowCaster(const OpenGLDrawItem& item) const;
	void RenderScreenSpaceQuad();
	PBRStandard* GetShader(MaterialShaderType shader_type);

	GLuint quad_VAO = 0;
	GLuint quad_VBO = 0;
	GLuint default_white_texture = 0;
	GLuint default_normal_texture = 0;

	RenderSystem* rendering = nullptr;
	int window_width = 0;
	int window_height = 0;

	OpenGLFrameResources frame_resources;
	OpenGLShadowSystem shadow_system;
	OpenGLRenderQueues render_queues;
	OpenGLShaderPasses shader_passes;
	OpenGLEnvironmentResources environment_resources;
	std::unordered_map<const MeshData*, OpenGLMeshResources> mesh_resources;
	std::unordered_map<Material*, OpenGLMaterialResources> material_resources;
	std::unordered_map<const Texture*, OpenGLLightmapResources> lightmap_resources;
	std::vector<OpenGLDrawItem> draw_items;
	std::vector<float> camera_distance_squared;
};


} // namespace CEngine::Renderer
#endif
