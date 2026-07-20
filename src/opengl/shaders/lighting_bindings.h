#ifndef LIGHTING_BINDINGS_H
#define LIGHTING_BINDINGS_H

#include <cstdint>

#include <glad/glad.h>

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

struct OpenGLAmbientUniforms
{
	GLint sky_color = -1;
	GLint ground_color = -1;
	GLint intensity = -1;
	GLint enabled = -1;

	void Initialize(GLuint shader_id);
	void Upload() const;
};

#endif
