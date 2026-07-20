#include "lighting_bindings.h"

#include "renderer/light.h"
#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

namespace CEngine::Renderer {

namespace {
constexpr GLuint kDirectLightBindingPoint = 0;
constexpr GLuint kShadowBindingPoint = 1;
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

OpenGLShadowBuffer::~OpenGLShadowBuffer()
{
	Destroy();
}

void OpenGLShadowBuffer::Initialize(GLuint shader_id, const char* block_name)
{
	const GLuint block_index = glGetUniformBlockIndex(shader_id, block_name);
	if (block_index != GL_INVALID_INDEX)
	{
		glUniformBlockBinding(shader_id, block_index, kShadowBindingPoint);
	}

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeof(OpenGLShadowGpuData)), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, kShadowBindingPoint, buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLShadowBuffer::Upload(const OpenGLShadowGpuData& data)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, kShadowBindingPoint, buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(OpenGLShadowGpuData)), &data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLShadowBuffer::Destroy()
{
	if (buffer != 0)
	{
		glDeleteBuffers(1, &buffer);
		buffer = 0;
	}
}

void OpenGLShadowSamplers::Initialize(GLuint shader_id)
{
	atlas = glGetUniformLocation(shader_id, "shadow_atlas");
	for (int index = 0; index < OpenGLShadows::kMaxPointShadows; ++index)
	{
		const std::string name = "point_shadow_maps[" + std::to_string(index) + "]";
		point_maps[index] = glGetUniformLocation(shader_id, name.c_str());
	}
}

void OpenGLShadowSamplers::Bind(GLuint atlas_texture,
	const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_textures) const
{
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glUniform1i(atlas, 4);

	for (int index = 0; index < OpenGLShadows::kMaxPointShadows; ++index)
	{
		glActiveTexture(GL_TEXTURE5 + index);
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_textures[index]);
		glUniform1i(point_maps[index], 5 + index);
	}
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

} // namespace CEngine::Renderer
