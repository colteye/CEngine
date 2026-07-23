#ifndef POINT_SHADOW_DEPTH_H
#define POINT_SHADOW_DEPTH_H

#include "renderer/material.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer {

class PointShadowDepth
{
public:
	PointShadowDepth();

	void Use() const;
	void UpdateFrame(const glm::mat4& view,
		const glm::mat4& projection, const glm::vec3& light_position,
		float far_plane);
	void UpdateObject(const glm::mat4& model,
		const Material& material, GLuint albedo_texture);

private:
	void InitializeParameters();

	ShaderProgram shader_program;
	GLint model_id = -1;
	GLint view_id = -1;
	GLint projection_id = -1;
	GLint light_position_id = -1;
	GLint far_plane_id = -1;
	GLint albedo_id = -1;
	GLint base_color_factor_id = -1;
	GLint alpha_cutoff_id = -1;
	GLint alpha_test_id = -1;
};


} // namespace CEngine::Renderer
#endif
