#include "deferred_lighting.h"

#include "renderer/render_system.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

DeferredLighting::DeferredLighting()
{
	shader_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/deferred_lighting.frag");
	InitializeParameters();
}

void DeferredLighting::Use() const
{
	shader_program.Use();
}

void DeferredLighting::Update(RenderSystem& rendering,
	GLuint albedo, GLuint normal_roughness, GLuint material, GLuint baked_light,
	GLuint depth, int width, int height, const OpenGLShadowGpuData& shadow_data, GLuint shadow_atlas,
	const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_shadow_maps,
	GLuint irradiance_map, GLuint prefiltered_map)
{
	const RenderFrameConstants& constants = rendering.GetFrameConstants();
	const glm::mat4 inverse_view = glm::inverse(constants.view);
	const glm::mat4 inverse_projection = glm::inverse(constants.proj);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, albedo);
	glUniform1i(albedo_id, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normal_roughness);
	glUniform1i(normal_roughness_id, 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, material);
	glUniform1i(material_id, 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, baked_light);
	glUniform1i(baked_light_id, 3);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, depth);
	glUniform1i(depth_id, 4);

	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(constants.view));
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
	glUniformMatrix4fv(inverse_view_id, 1, GL_FALSE, glm::value_ptr(inverse_view));
	glUniformMatrix4fv(inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
	glUniform3fv(camera_position_id, 1, glm::value_ptr(constants.camera_position));
	glUniform2f(texel_size_id, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

	ambient_uniforms.Upload(rendering);
	direct_lights.BindAndUploadIfNeeded(rendering);
	shadow_buffer.Upload(shadow_data);
	shadow_samplers.Bind(shadow_atlas, point_shadow_maps);
	environment_uniforms.BindAndUpload(
		rendering, irradiance_map, prefiltered_map);
}

void DeferredLighting::InitializeParameters()
{
	const GLuint shader_id = shader_program.GetId();
	direct_lights.Initialize(shader_id, "DirectLightBlock");
	shadow_buffer.Initialize(shader_id, "ShadowBlock");
	shadow_samplers.Initialize(shader_id);
	ambient_uniforms.Initialize(shader_id);
	environment_uniforms.Initialize(shader_id);

	albedo_id = glGetUniformLocation(shader_id, "g_albedo");
	normal_roughness_id = glGetUniformLocation(shader_id, "g_normal_roughness");
	material_id = glGetUniformLocation(shader_id, "g_material");
	baked_light_id = glGetUniformLocation(shader_id, "g_baked_light");
	depth_id = glGetUniformLocation(shader_id, "g_depth");
	view_id = glGetUniformLocation(shader_id, "view");
	projection_id = glGetUniformLocation(shader_id, "projection");
	inverse_view_id = glGetUniformLocation(shader_id, "inverse_view");
	inverse_projection_id = glGetUniformLocation(shader_id, "inverse_projection");
	camera_position_id = glGetUniformLocation(shader_id, "cam_pos_world");
	texel_size_id = glGetUniformLocation(shader_id, "texel_size");
}

} // namespace CEngine::Renderer
