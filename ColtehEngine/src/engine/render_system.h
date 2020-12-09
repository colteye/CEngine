#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "model.h"
#include "light.h"
#include "shaders/shader.h"
#include <string>
#include <vector>
#include <glad/glad.h>

class RenderSystem
{
public:
	static void Initialize();
	static void Render();

	static void RegisterModel(Model *model);
	static void RegisterShader(Shader * shader);

	static unsigned int RegisterLight(glm::vec3 light_pos, glm::vec3 light_col, float light_pow);

	static const std::vector<glm::vec3>& GetLightPositions();
	static const std::vector<glm::vec3>& GetLightColors();
	static const std::vector<float>& GetLightPowers();

private:
	static GLuint vertex_buffer;
	static GLuint uv_buffer;
	static GLuint normal_buffer;
	static GLuint tangent_buffer;

	static size_t vertex_buffer_length;
	static size_t uv_buffer_length;
	static size_t normal_buffer_length;
	static size_t tangent_buffer_length;

	static std::vector<Model*> model_list;	
	static std::vector<Shader*> shader_list;

	// These properties must be separated for OpenGL.
	static std::vector<glm::vec3> light_pos_list;
	static std::vector<glm::vec3> light_col_list;
	static std::vector<float> light_pow_list;
};
#endif