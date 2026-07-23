#include "shader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace CEngine::Renderer {
namespace {

bool ReadShaderSource(const std::filesystem::path& path,
	std::unordered_set<std::string>& includes, std::string& output)
{
	const std::filesystem::path normalized = path.lexically_normal();
	const std::string key = normalized.generic_string();
	if (!includes.emplace(key).second)
	{
		std::cerr << "Shader include cycle contains: " << key << '\n';
		return false;
	}

	std::ifstream stream(normalized);
	if (!stream)
	{
		std::cerr << "Could not open shader: " << key << '\n';
		includes.erase(key);
		return false;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		const std::size_t first = line.find_first_not_of(" \t");
		if (first != std::string::npos &&
			line.compare(first, 10, "#include \"") == 0)
		{
			const std::size_t end = line.find('"', first + 10);
			if (end == std::string::npos ||
				!ReadShaderSource(normalized.parent_path() /
					line.substr(first + 10, end - first - 10),
					includes, output))
			{
				includes.erase(key);
				return false;
			}
			continue;
		}
		output += line;
		output.push_back('\n');
	}

	includes.erase(key);
	return true;
}

bool LoadShaderSource(
	const std::filesystem::path& path, std::string& output)
{
	std::unordered_set<std::string> includes;
	return ReadShaderSource(path, includes, output);
}

bool CompileShader(GLuint shader, const std::string& source,
	const std::string& path)
{
	std::cout << "Compiling shader: " << path << '\n';
	const char* source_pointer = source.c_str();
	glShaderSource(shader, 1, &source_pointer, nullptr);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	GLint log_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 1)
	{
		std::vector<char> log(static_cast<std::size_t>(log_length));
		glGetShaderInfoLog(shader, log_length, nullptr, log.data());
		std::cerr << log.data() << '\n';
	}
	return compiled == GL_TRUE;
}

} // namespace

ShaderProgram::~ShaderProgram()
{
	if (program_id != 0)
	{
		glDeleteProgram(program_id);
		program_id = 0;
	}
}

bool ShaderProgram::Load(const std::string& vertex_file_path, const std::string& fragment_file_path)
{
	GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_id = glCreateShader(GL_FRAGMENT_SHADER);
	std::string vertex_code;
	std::string fragment_code;
	if (!LoadShaderSource(vertex_file_path, vertex_code) ||
		!LoadShaderSource(fragment_file_path, fragment_code) ||
		!CompileShader(vertex_id, vertex_code, vertex_file_path) ||
		!CompileShader(fragment_id, fragment_code, fragment_file_path))
	{
		glDeleteShader(vertex_id);
		glDeleteShader(fragment_id);
		return false;
	}

	const GLuint linked_program = glCreateProgram();
	glAttachShader(linked_program, vertex_id);
	glAttachShader(linked_program, fragment_id);
	glLinkProgram(linked_program);

	GLint linked = GL_FALSE;
	glGetProgramiv(linked_program, GL_LINK_STATUS, &linked);
	GLint log_length = 0;
	glGetProgramiv(linked_program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 1)
	{
		std::vector<char> log(static_cast<std::size_t>(log_length));
		glGetProgramInfoLog(
			linked_program, log_length, nullptr, log.data());
		std::cerr << log.data() << '\n';
	}

	glDetachShader(linked_program, vertex_id);
	glDetachShader(linked_program, fragment_id);
	glDeleteShader(vertex_id);
	glDeleteShader(fragment_id);

	if (linked != GL_TRUE)
	{
		glDeleteProgram(linked_program);
		return false;
	}
	if (program_id != 0) glDeleteProgram(program_id);
	program_id = linked_program;
	return true;
}

} // namespace CEngine::Renderer
