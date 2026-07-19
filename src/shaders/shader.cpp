#include <fstream>
#include <sstream>
#include <vector>

#include "shader.h"

void Shader::Load() {

	SetShaderFiles();

	// Create the shaders
	GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_id = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string vertex_code;
	std::ifstream vertex_stream(vertex_file_path, std::ios::in);

	if (vertex_stream.is_open()) 
	{
		std::stringstream sstr;
		sstr << vertex_stream.rdbuf();
		vertex_code = sstr.str();
		vertex_stream.close();
	}
	else 
	{
		printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_file_path.c_str());
		getchar();
	}

	// Read the Fragment Shader code from the file
	std::string fragment_code;
	std::ifstream fragment_stream(fragment_file_path, std::ios::in);
	if (fragment_stream.is_open()) 
	{
		std::stringstream sstr;
		sstr << fragment_stream.rdbuf();
		fragment_code = sstr.str();
		fragment_stream.close();
	}

	GLint result = GL_FALSE;
	int log_length;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path.c_str());
	char const * vertex_src_ptr = vertex_code.c_str();
	glShaderSource(vertex_id, 1, &vertex_src_ptr, nullptr);
	glCompileShader(vertex_id);

	// Check Vertex Shader
	glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(vertex_id, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) 
	{
		std::vector<char> vertex_error_msg(log_length + 1);
		glGetShaderInfoLog(vertex_id, log_length, nullptr, &vertex_error_msg[0]);
		printf("%s\n", &vertex_error_msg[0]);
	}

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path.c_str());
	char const * fragment_src_ptr = fragment_code.c_str();
	glShaderSource(fragment_id, 1, &fragment_src_ptr, nullptr);
	glCompileShader(fragment_id);

	// Check Fragment Shader
	glGetShaderiv(fragment_id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(fragment_id, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) 
	{
		std::vector<char> fragment_error_msg(log_length + 1);
		glGetShaderInfoLog(fragment_id, log_length, nullptr, &fragment_error_msg[0]);
		printf("%s\n", &fragment_error_msg[0]);
	}

	// Link the program
	printf("Linking program\n");
	shader_id = glCreateProgram();
	glAttachShader(shader_id, vertex_id);
	glAttachShader(shader_id, fragment_id);
	glLinkProgram(shader_id);

	// Check the program
	glGetProgramiv(shader_id, GL_LINK_STATUS, &result);
	glGetProgramiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) 
	{
		std::vector<char> prog_error_msg(log_length + 1);
		glGetProgramInfoLog(shader_id, log_length, nullptr, &prog_error_msg[0]);
		printf("%s\n", &prog_error_msg[0]);
	}

	glDetachShader(shader_id, vertex_id);
	glDetachShader(shader_id, fragment_id);

	glDeleteShader(vertex_id);
	glDeleteShader(fragment_id);

	InitializeParameters();
}