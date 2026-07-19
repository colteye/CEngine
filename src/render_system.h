#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "light.h"
#include "model.h"
#include "shaders/shader.h"
#include "shaders/ssao.h"

#include <glad/glad.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Draw ranges for one material in a shader-owned vertex buffer.
// 1 VBO per shader, 1 draw call per material.
struct MaterialRenderData {
	std::vector<GLint> start_indexes;
	std::vector<GLsizei> counts;
};

struct ShaderBuffers {
	ShaderBuffers() = default;
	ShaderBuffers(const ShaderBuffers&) = delete;
	ShaderBuffers& operator=(const ShaderBuffers&) = delete;
	ShaderBuffers(ShaderBuffers&& other) noexcept;
	ShaderBuffers& operator=(ShaderBuffers&& other) noexcept;
	~ShaderBuffers();

	void Destroy();

	// Materials belonging to each shader.
	std::vector<Material*> materials;

	GLuint vertex_array_obj = 0;

	// References for the buffers in OpenGL.
	GLuint vertex_buffer = 0;
	GLuint uv_buffer = 0;
	GLuint normal_buffer = 0;
	GLuint tangent_buffer = 0;
	GLuint indice_buffer = 0;

	// Length of each buffer.
	size_t vertex_buffer_length = 0;
	size_t uv_buffer_length = 0;
	size_t normal_buffer_length = 0;
	size_t tangent_buffer_length = 0;
	size_t indice_buffer_length = 0;
};

class RenderSystem
{
public:
	static void Initialize(int window_width, int window_height);
	static void Shutdown();
	
	static void Render();

	static void RegisterMesh(const Mesh* mesh);
	static void RegisterMaterial(Material* material);

	static unsigned int RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static void UpdateLight(unsigned int id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);

	static const std::vector<glm::vec3>& GetLightPositions();
	static const std::vector<glm::vec3>& GetLightColors();
	static const std::vector<float>& GetLightPowers();

private:

	static GLuint g_buffer, g_color, rbo_depth;
	static GLuint quad_VAO, quad_VBO;
	static std::unique_ptr<SSAO> ssao;

	static int window_width, window_height;

	static ShaderBuffers InitShaderBuffers(Shader* shader);

	static void RenderScreenSpaceQuad();

	static std::unordered_map<Shader*, ShaderBuffers> shader_buffer_dict;
	static std::unordered_map<Material*, MaterialRenderData> material_render_dict;

	// These properties must be separated for OpenGL.
	static std::vector<glm::vec3> light_pos_list;
	static std::vector<glm::vec3> light_col_list;
	static std::vector<float> light_pow_list;
};
#endif
