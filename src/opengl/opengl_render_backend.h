#ifndef OPENGL_RENDER_BACKEND_H
#define OPENGL_RENDER_BACKEND_H

#include "material.h"
#include "render_backend.h"
#include "opengl/shaders/pbr_standard.h"
#include "opengl/shaders/ssao.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>
#include <vector>

struct MaterialRenderData {
	std::vector<GLint> start_indexes;
	std::vector<GLsizei> counts;
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

	std::vector<Material*> materials;

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
	void RegisterRenderable(const Renderable& renderable) override;
	void RegisterMaterial(Material* material) override;

private:
	static ShaderBuffers InitShaderBuffers();

	void RenderScreenSpaceQuad();
	PBRStandard* GetShader(MaterialShaderType shader_type);

	GLuint g_buffer = 0;
	GLuint g_color = 0;
	GLuint rbo_depth = 0;
	GLuint quad_VAO = 0;
	GLuint quad_VBO = 0;

	int window_width = 0;
	int window_height = 0;

	std::unique_ptr<SSAO> ssao;
	std::unique_ptr<PBRStandard> pbr_shader;
	std::unordered_map<MaterialShaderType, ShaderBuffers> shader_buffer_dict;
	std::unordered_map<Material*, MaterialRenderData> material_render_dict;
	std::unordered_map<Material*, OpenGLMaterialResources> material_resources;
};

#endif
