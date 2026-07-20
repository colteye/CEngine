#ifndef LIGHTING_BINDINGS_H
#define LIGHTING_BINDINGS_H

#include "renderer/opengl/opengl_shadow_types.h"

#include <array>
#include <cstdint>

#include <glad/glad.h>

namespace CEngine::Renderer {

class OpenGLDirectLightBuffer
{
public:
	OpenGLDirectLightBuffer() = default;
	OpenGLDirectLightBuffer(const OpenGLDirectLightBuffer&) = delete;
	OpenGLDirectLightBuffer& operator=(const OpenGLDirectLightBuffer&) = delete;
	~OpenGLDirectLightBuffer();

	void Initialize(GLuint shader_id, const char* block_name);
	void BindAndUploadIfNeeded();
	void Destroy();

private:
	GLuint buffer = 0;
	uint64_t uploaded_revision = 0;
};

class OpenGLShadowBuffer
{
public:
	OpenGLShadowBuffer() = default;
	OpenGLShadowBuffer(const OpenGLShadowBuffer&) = delete;
	OpenGLShadowBuffer& operator=(const OpenGLShadowBuffer&) = delete;
	~OpenGLShadowBuffer();

	void Initialize(GLuint shader_id, const char* block_name);
	void Upload(const OpenGLShadowGpuData& data);
	void Destroy();

private:
	GLuint buffer = 0;
};

struct OpenGLShadowSamplers
{
	GLint atlas = -1;
	std::array<GLint, OpenGLShadows::kMaxPointShadows> point_maps {};

	void Initialize(GLuint shader_id);
	void Bind(GLuint atlas_texture, const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_textures) const;
};

struct OpenGLAmbientUniforms
{
	GLint sky_color = -1;
	GLint ground_color = -1;
	GLint intensity = -1;
	GLint enabled = -1;

	void Initialize(GLuint shader_id);
	void Upload() const;
};


} // namespace CEngine::Renderer
#endif
