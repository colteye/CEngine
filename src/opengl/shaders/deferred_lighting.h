#ifndef DEFERRED_LIGHTING_H
#define DEFERRED_LIGHTING_H

#include "lighting_bindings.h"
#include "shader.h"

#include <glm/glm.hpp>

class DeferredLighting
{
public:
	DeferredLighting();

	void Use() const;
	void Update(GLuint albedo, GLuint normal_roughness, GLuint material, GLuint depth, int width, int height);

private:
	void InitializeParameters();

	ShaderProgram shader_program;
	OpenGLDirectLightBuffer direct_lights;
	OpenGLAmbientUniforms ambient_uniforms;

	GLint albedo_id = -1;
	GLint normal_roughness_id = -1;
	GLint material_id = -1;
	GLint depth_id = -1;
	GLint view_id = -1;
	GLint projection_id = -1;
	GLint inverse_view_id = -1;
	GLint inverse_projection_id = -1;
	GLint camera_position_id = -1;
	GLint texel_size_id = -1;
};

#endif
