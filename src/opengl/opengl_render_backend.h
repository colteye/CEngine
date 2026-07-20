#ifndef OPENGL_RENDER_BACKEND_H
#define OPENGL_RENDER_BACKEND_H

#include "material.h"
#include "render_backend.h"
#include "renderable.h"
#include "opengl/shaders/deferred_lighting.h"
#include "opengl/shaders/depth_only.h"
#include "opengl/shaders/pbr_geometry_pass.h"
#include "opengl/shaders/pbr_standard.h"
#include "opengl/shaders/ssao.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>
#include <vector>

struct OpenGLDrawItem {
	Material* material = nullptr;
	glm::mat4 transform = glm::mat4(1.0f);
	Bounds world_bounds;
	GLint start_index = 0;
	GLsizei count = 0;
	uint32_t flags = 0;
};

struct OpenGLMaterialResources {
	GLuint albedo_tex = 0;
	GLuint normal_tex = 0;
	GLuint metallic_roughness_ao_tex = 0;

	void Destroy();
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
	GLuint normal_buffer = 0;
	GLuint tangent_buffer = 0;
	GLuint indice_buffer = 0;

	size_t vertex_buffer_length = 0;
	size_t uv_buffer_length = 0;
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
	void RegisterMaterial(Material* material) override;

private:
	static ShaderBuffers InitShaderBuffers();

	bool CreateFrameResources(int width, int height);
	void DestroyFrameResources();
	void BuildRenderQueues();
	void RenderGeometryPass();
	void RenderDeferredLightingPass();
	void RenderForwardQueue(const std::vector<uint32_t>& queue);
	void DrawItem(const OpenGLDrawItem& item) const;
	bool DrawsInDeferredGeometry(MaterialRenderMode mode) const;
	bool DrawsInForward(MaterialRenderMode mode) const;
	bool DrawsTransparent(MaterialRenderMode mode) const;
	bool DrawsShadowCaster(const OpenGLDrawItem& item) const;
	void RenderScreenSpaceQuad();
	PBRStandard* GetShader(MaterialShaderType shader_type);

	GLuint g_buffer_fbo = 0;
	GLuint g_albedo = 0;
	GLuint g_normal_roughness = 0;
	GLuint g_material = 0;
	GLuint scene_depth = 0;
	GLuint lit_color_fbo = 0;
	GLuint lit_color = 0;
	GLuint depth_only_fbo = 0;
	GLuint quad_VAO = 0;
	GLuint quad_VBO = 0;

	int window_width = 0;
	int window_height = 0;

	std::unique_ptr<SSAO> ssao;
	std::unique_ptr<PBRStandard> pbr_shader;
	std::unique_ptr<PBRGeometryPass> pbr_geometry_pass;
	std::unique_ptr<DeferredLighting> deferred_lighting;
	std::unique_ptr<DepthOnly> depth_only_shader;
	std::unordered_map<MaterialShaderType, ShaderBuffers> shader_buffer_dict;
	std::unordered_map<Material*, OpenGLMaterialResources> material_resources;
	std::vector<OpenGLDrawItem> draw_items;
	std::vector<uint32_t> opaque_deferred_queue;
	std::vector<uint32_t> forward_queue;
	std::vector<uint32_t> transparent_queue;
	std::vector<uint32_t> shadow_caster_queue;
};

#endif
