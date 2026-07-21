#ifndef SSAO_H
#define SSAO_H

#include "shader.h"

namespace CEngine::Renderer {

class SSAO
{
public:

	SSAO();
	void UseCompute() const;
	void UpdateCompute();
	void UseComposite() const;
	void UpdateComposite();
	void SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, GLuint ao, int width, int height);

private:
	void InitializeParameters();
	ShaderProgram compute_program;
	ShaderProgram composite_program;
	GLuint render_tex = 0;
	GLuint depth_tex = 0;
	GLuint normal_roughness_tex = 0;
	GLuint ao_tex = 0;
	GLint depth_id = -1;
	GLint normal_roughness_id = -1;
	GLint projection_id = -1;
	GLint inverse_projection_id = -1;
	GLint view_id = -1;
	GLint composite_render_id = -1;
	GLint composite_depth_id = -1;
	GLint composite_ao_id = -1;
	GLint texel_size_id = -1;

	int texture_width;
	int texture_height;

};


} // namespace CEngine::Renderer
#endif
