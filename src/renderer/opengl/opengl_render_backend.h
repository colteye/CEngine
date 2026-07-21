#ifndef OPENGL_RENDER_BACKEND_H
#define OPENGL_RENDER_BACKEND_H

#include "renderer/material.h"
#include "renderer/render_backend.h"
#include "renderer/renderable.h"
#include "renderer/opengl/opengl_render_data.h"
#include "renderer/opengl/opengl_shadow_system.h"
#include "renderer/opengl/shaders/deferred_lighting.h"
#include "renderer/opengl/shaders/depth_only.h"
#include "renderer/opengl/shaders/fullscreen_blit.h"
#include "renderer/opengl/shaders/pbr_geometry_pass.h"
#include "renderer/opengl/shaders/pbr_standard.h"
#include "renderer/opengl/shaders/ssao.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace CEngine::Renderer {

struct OpenGLMaterialResources {
	GLuint albedo_tex = 0;
	GLuint normal_tex = 0;
	GLuint metallic_roughness_ao_tex = 0;

	void Destroy();
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
	GLuint depth_only_fbo = 0;

	void Destroy();
};

struct OpenGLShaderPasses {
	std::unique_ptr<SSAO> ssao;
	std::unique_ptr<PBRStandard> pbr_forward;
	std::unique_ptr<PBRGeometryPass> pbr_geometry;
	std::unique_ptr<DeferredLighting> deferred_lighting;
	std::unique_ptr<DepthOnly> depth_only;
	std::unique_ptr<FullscreenBlit> fullscreen_blit;
};

struct ShaderBuffers {
	ShaderBuffers() = default;
	ShaderBuffers(const ShaderBuffers&) = delete;
	ShaderBuffers& operator=(const ShaderBuffers&) = delete;
	ShaderBuffers(ShaderBuffers&& other) noexcept;
	ShaderBuffers& operator=(ShaderBuffers&& other) noexcept;
	~ShaderBuffers();

	void Destroy();

	GLuint vertex_array_obj = 0;
	GLuint vertex_buffer = 0;
	GLuint uv_buffer = 0;
	GLuint lightmap_uv_buffer = 0;
	GLuint normal_buffer = 0;
	GLuint tangent_buffer = 0;
	GLuint indice_buffer = 0;

	size_t vertex_buffer_length = 0;
	size_t uv_buffer_length = 0;
	size_t lightmap_uv_buffer_length = 0;
	size_t normal_buffer_length = 0;
	size_t tangent_buffer_length = 0;
	size_t indice_buffer_length = 0;
};

class OpenGLRenderBackend final : public IRenderBackend
{
public:
	bool Initialize(GLFWwindow* window, int window_width, int window_height) override;
	void Shutdown() override;
	void Render() override;
	void RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
		uint32_t native_depth_texture, int texture_width, int texture_height) override;
	void RegisterRenderable(const Renderable& renderable) override;
	void RemoveRenderable(RenderableHandle handle) override;
	void UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform,
		const Bounds& world_bounds) override;
	void RegisterMaterial(Material* material) override;
	void RemoveMaterial(Material* material) override;
	bool RegisterLightmap(const Lightmap* lightmap) override;
	void RemoveLightmap(const Lightmap* lightmap) override;

private:
	static ShaderBuffers InitShaderBuffers();

	bool CreateFrameResources(int width, int height);
	void DestroyFrameResources();
	void BuildRenderQueues();
	void RenderGeometryPass();
	void RenderDeferredLightingPass();
	void RenderAmbientOcclusionPass();
	void RenderForwardQueue(const std::vector<uint32_t>& queue, bool transparent);
	void PresentSceneColor();
	void DrawItem(const OpenGLDrawItem& item) const;
	OpenGLRenderQueue ClassifyRenderMode(MaterialRenderMode mode) const;
	bool DrawsShadowCaster(const OpenGLDrawItem& item) const;
	void RenderScreenSpaceQuad();
	PBRStandard* GetShader(MaterialShaderType shader_type);

	GLuint quad_VAO = 0;
	GLuint quad_VBO = 0;

	int window_width = 0;
	int window_height = 0;

	OpenGLFrameResources frame_resources;
	OpenGLShadowSystem shadow_system;
	OpenGLRenderQueues render_queues;
	OpenGLShaderPasses shader_passes;
	std::unordered_map<MaterialShaderType, ShaderBuffers> shader_buffer_dict;
	std::unordered_map<Material*, OpenGLMaterialResources> material_resources;
	std::unordered_map<const Lightmap*, GLuint> lightmap_resources;
	std::vector<OpenGLDrawItem> draw_items;
};


} // namespace CEngine::Renderer
#endif
