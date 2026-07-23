#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "lighting_bindings.h"
#include "shader.h"

#include "renderer/material.h"

namespace CEngine::Renderer {

class RenderSystem;

class PBRStandard
{
public:

	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;
	GLuint lightmap_tex;

	PBRStandard();
	void Use() const;
	void UpdateFrame(RenderSystem& rendering,
		const OpenGLShadowGpuData& shadow_data,
		GLuint shadow_atlas, const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_shadow_maps,
		GLuint irradiance_map, GLuint prefiltered_map);
	void UpdateObject(const glm::mat4& model, const Material& material, const glm::vec2& lightmap_scale,
		const glm::vec2& lightmap_offset, float lightmap_rgbm_range);
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetMaterialParameters(const Material& material);

	ShaderProgram shader_program;
	OpenGLDirectLightBuffer direct_lights;
	OpenGLShadowBuffer shadow_buffer;
	OpenGLShadowSamplers shadow_samplers;
	OpenGLAmbientUniforms ambient_uniforms;
	OpenGLEnvironmentUniforms environment_uniforms;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;
	GLuint base_color_factor_id;
	GLuint metallic_roughness_ao_factors_id;
	GLuint alpha_cutoff_id;
	GLuint render_mode_id;
	GLuint receives_shadows_id;
	GLuint lightmap_id;
	GLuint lightmap_scale_offset_id;
	GLuint lightmap_rgbm_range_id;
	GLuint has_lightmap_id;

};


} // namespace CEngine::Renderer
#endif
