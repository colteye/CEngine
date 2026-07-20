#ifndef PBR_GEOMETRY_PASS_H
#define PBR_GEOMETRY_PASS_H

#include "renderer/material.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer {

class PBRGeometryPass
{
public:
	PBRGeometryPass();

	void Use() const;
	void Update(const glm::mat4& model, const Material& material);
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

private:
	void InitializeParameters();

	ShaderProgram shader_program;
	GLuint albedo_tex = 0;
	GLuint normal_tex = 0;
	GLuint metallic_roughness_ao_tex = 0;

	GLint model_id = -1;
	GLint view_id = -1;
	GLint projection_id = -1;
	GLint albedo_id = -1;
	GLint normal_id = -1;
	GLint metallic_roughness_ao_id = -1;
	GLint base_color_factor_id = -1;
	GLint alpha_cutoff_id = -1;
	GLint render_mode_id = -1;
	GLint receives_shadows_id = -1;
};


} // namespace CEngine::Renderer
#endif
