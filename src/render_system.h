#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "model.h"
#include "light.h"
#include "shaders/shader.h"
#include <string>
#include <vector>
#include <glad/glad.h>
#include "shaders/ssao.h"


#include <iostream>

// Struct of data for use when rendering.
// 1 VBO per shader, 1 draw call per material.
struct MaterialRenderData {
	std::vector<MeshData*> mesh_data = std::vector<MeshData*>();
	std::vector<GLint> start_indexes = std::vector<GLint>();
	std::vector<GLsizei> counts = std::vector<GLsizei>();
};

struct ShaderBuffers {
	// Materials belonging to each shader.
	std::vector<Material*> materials = std::vector<Material*>();

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
	
	static void Render();

	static void RegisterMesh(Mesh * mesh);
	static void RegisterMaterial(Material * material);

	static unsigned int RegisterLight(glm::vec3 light_pos, glm::vec3 light_col, float light_pow);
	static void UpdateLight(unsigned int id, glm::vec3 light_pos, glm::vec3 light_col, float light_pow);

	static const std::vector<glm::vec3>& GetLightPositions();
	static const std::vector<glm::vec3>& GetLightColors();
	static const std::vector<float>& GetLightPowers();

private:

	static GLuint g_buffer, g_position, g_normal, g_color, rbo_depth;
	static GLuint quad_VAO, quad_VBO;
	static SSAO *ssao;

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