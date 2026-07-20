#include "deferred_lighting.h"

#include "light.h"
#include "render_system.h"

#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr GLuint kDirectLightBindingPoint = 0;
}

DeferredLighting::DeferredLighting()
{
	shader_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/deferred_lighting.frag");
	InitializeParameters();
}

DeferredLighting::~DeferredLighting()
{
	if (light_ubo != 0)
	{
		glDeleteBuffers(1, &light_ubo);
		light_ubo = 0;
	}
}

void DeferredLighting::Use() const
{
	shader_program.Use();
}

void DeferredLighting::Update(GLuint albedo, GLuint normal_roughness, GLuint material, GLuint depth,
	int width, int height)
{
	const RenderFrameConstants& constants = RenderSystem::GetFrameConstants();
	const AmbientLighting& ambient = RenderSystem::GetAmbientLighting();
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
	glBindTexture(GL_TEXTURE_2D, depth);
	glUniform1i(depth_id, 3);

	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(constants.view));
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
	glUniformMatrix4fv(inverse_view_id, 1, GL_FALSE, glm::value_ptr(inverse_view));
	glUniformMatrix4fv(inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
	glUniform3fv(camera_position_id, 1, glm::value_ptr(constants.camera_position));
	glUniform2f(texel_size_id, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
	glUniform3fv(ambient_sky_color_id, 1, glm::value_ptr(ambient.sky_color));
	glUniform3fv(ambient_ground_color_id, 1, glm::value_ptr(ambient.ground_color));
	glUniform1f(ambient_intensity_id, ambient.intensity);
	glUniform1i(ambient_enabled_id, ambient.enabled ? 1 : 0);

	UploadLights();
}

void DeferredLighting::InitializeParameters()
{
	const GLuint shader_id = shader_program.GetId();
	const GLuint light_block_index = glGetUniformBlockIndex(shader_id, "DirectLightBlock");
	if (light_block_index != GL_INVALID_INDEX)
	{
		glUniformBlockBinding(shader_id, light_block_index, kDirectLightBindingPoint);
	}

	glGenBuffers(1, &light_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
	glBufferData(GL_UNIFORM_BUFFER,
		static_cast<GLsizeiptr>(sizeof(glm::vec4) + RenderSystem::GetMaxGpuLights() * sizeof(GpuLight)),
		nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, kDirectLightBindingPoint, light_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	albedo_id = glGetUniformLocation(shader_id, "g_albedo");
	normal_roughness_id = glGetUniformLocation(shader_id, "g_normal_roughness");
	material_id = glGetUniformLocation(shader_id, "g_material");
	depth_id = glGetUniformLocation(shader_id, "g_depth");
	view_id = glGetUniformLocation(shader_id, "view");
	projection_id = glGetUniformLocation(shader_id, "projection");
	inverse_view_id = glGetUniformLocation(shader_id, "inverse_view");
	inverse_projection_id = glGetUniformLocation(shader_id, "inverse_projection");
	camera_position_id = glGetUniformLocation(shader_id, "cam_pos_world");
	texel_size_id = glGetUniformLocation(shader_id, "texel_size");
	ambient_sky_color_id = glGetUniformLocation(shader_id, "ambient_sky_color");
	ambient_ground_color_id = glGetUniformLocation(shader_id, "ambient_ground_color");
	ambient_intensity_id = glGetUniformLocation(shader_id, "ambient_intensity");
	ambient_enabled_id = glGetUniformLocation(shader_id, "ambient_enabled");
}

void DeferredLighting::UploadLights()
{
	const uint64_t light_revision = RenderSystem::GetLightRevision();
	glBindBufferBase(GL_UNIFORM_BUFFER, kDirectLightBindingPoint, light_ubo);
	if (uploaded_light_revision == light_revision)
	{
		return;
	}

	const std::vector<GpuLight>& gpu_lights = RenderSystem::GetGpuLights();
	const glm::vec4 light_info(static_cast<float>(gpu_lights.size()), 0.0f, 0.0f, 0.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(light_info));
	if (!gpu_lights.empty())
	{
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4),
			static_cast<GLsizeiptr>(gpu_lights.size() * sizeof(GpuLight)), gpu_lights.data());
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	uploaded_light_revision = light_revision;
}
