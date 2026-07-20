#ifndef DEFERRED_LIGHTING_H
#define DEFERRED_LIGHTING_H

#include "shader.h"

#include <cstdint>

#include <glm/glm.hpp>

class DeferredLighting
{
public:
	DeferredLighting();
	~DeferredLighting();

	void Use() const;
	void Update(GLuint albedo, GLuint normal_roughness, GLuint material, GLuint depth, int width, int height);

private:
	void InitializeParameters();
	void UploadLights();

	ShaderProgram shader_program;
	GLuint light_ubo = 0;
	uint64_t uploaded_light_revision = 0;

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
	GLint ambient_sky_color_id = -1;
	GLint ambient_ground_color_id = -1;
	GLint ambient_intensity_id = -1;
	GLint ambient_enabled_id = -1;
};

#endif
