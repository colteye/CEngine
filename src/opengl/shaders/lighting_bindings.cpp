#include "lighting_bindings.h"

#include "light.h"
#include "render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr GLuint kDirectLightBindingPoint = 0;
}

OpenGLDirectLightBuffer::~OpenGLDirectLightBuffer()
{
	Destroy();
}

void OpenGLDirectLightBuffer::Initialize(GLuint shader_id, const char* block_name)
{
	const GLuint block_index = glGetUniformBlockIndex(shader_id, block_name);
	if (block_index != GL_INVALID_INDEX)
	{
		glUniformBlockBinding(shader_id, block_index, kDirectLightBindingPoint);
	}

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferData(GL_UNIFORM_BUFFER,
		static_cast<GLsizeiptr>(sizeof(glm::vec4) + RenderSystem::GetMaxGpuLights() * sizeof(GpuLight)),
		nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, kDirectLightBindingPoint, buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLDirectLightBuffer::BindAndUploadIfNeeded()
{
	glBindBufferBase(GL_UNIFORM_BUFFER, kDirectLightBindingPoint, buffer);

	const uint64_t light_revision = RenderSystem::GetLightRevision();
	if (uploaded_revision == light_revision)
	{
		return;
	}

	const std::vector<GpuLight>& gpu_lights = RenderSystem::GetGpuLights();
	const glm::vec4 light_info(static_cast<float>(gpu_lights.size()), 0.0f, 0.0f, 0.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(light_info));
	if (!gpu_lights.empty())
	{
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4),
			static_cast<GLsizeiptr>(gpu_lights.size() * sizeof(GpuLight)), gpu_lights.data());
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	uploaded_revision = light_revision;
}

void OpenGLDirectLightBuffer::Destroy()
{
	if (buffer != 0)
	{
		glDeleteBuffers(1, &buffer);
		buffer = 0;
	}
	uploaded_revision = 0;
}

void OpenGLAmbientUniforms::Initialize(GLuint shader_id)
{
	sky_color = glGetUniformLocation(shader_id, "ambient_sky_color");
	ground_color = glGetUniformLocation(shader_id, "ambient_ground_color");
	intensity = glGetUniformLocation(shader_id, "ambient_intensity");
	enabled = glGetUniformLocation(shader_id, "ambient_enabled");
}

void OpenGLAmbientUniforms::Upload() const
{
	const AmbientLighting& ambient = RenderSystem::GetAmbientLighting();
	glUniform3fv(sky_color, 1, glm::value_ptr(ambient.sky_color));
	glUniform3fv(ground_color, 1, glm::value_ptr(ambient.ground_color));
	glUniform1f(intensity, ambient.intensity);
	glUniform1i(enabled, ambient.enabled ? 1 : 0);
}
