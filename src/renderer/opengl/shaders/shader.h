#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <glad/glad.h>

namespace CEngine::Renderer {

class ShaderProgram
{
public:
	ShaderProgram() = default;
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;
	~ShaderProgram();

	bool Load(const std::string& vertex_file_path, const std::string& fragment_file_path);
	void Use() const { glUseProgram(program_id); }
	GLuint GetId() const { return program_id; }

private:
	GLuint program_id = 0;
};


} // namespace CEngine::Renderer
#endif
