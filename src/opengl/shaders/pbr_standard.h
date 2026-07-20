#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "lighting_bindings.h"
#include "shader.h"

#include "material.h"

class PBRStandard
{
public:

	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;

	PBRStandard();
	void Use() const;
	void Update(const glm::mat4& model, const Material& material);
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetMaterialParameters(const Material& material);
	void SetParametersDynamic(const glm::mat4& model);

	ShaderProgram shader_program;
	OpenGLDirectLightBuffer direct_lights;
	OpenGLAmbientUniforms ambient_uniforms;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;
	GLuint base_color_factor_id;
	GLuint alpha_cutoff_id;
	GLuint render_mode_id;

};

#endif
