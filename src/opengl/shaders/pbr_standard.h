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
	void Update(const glm::mat4& model, const Material& material, const OpenGLShadowGpuData& shadow_data,
		GLuint shadow_atlas, const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_shadow_maps);
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetMaterialParameters(const Material& material);
	void SetParametersDynamic(const glm::mat4& model);

	ShaderProgram shader_program;
	OpenGLDirectLightBuffer direct_lights;
	OpenGLShadowBuffer shadow_buffer;
	OpenGLShadowSamplers shadow_samplers;
	OpenGLAmbientUniforms ambient_uniforms;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;
	GLuint base_color_factor_id;
	GLuint alpha_cutoff_id;
	GLuint render_mode_id;
	GLuint receives_shadows_id;

};

#endif
