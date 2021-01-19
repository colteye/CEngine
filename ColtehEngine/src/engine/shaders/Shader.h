#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <glad/glad.h>

class Shader
{
public:
	void Load();
	void Use() { glUseProgram(shader_id); }
	void Update() 
	{ 
		SetParametersStatic();
		SetParametersDynamic();
	};

protected:
	virtual void SetShaderFiles() = 0;
	virtual void InitializeParameters() = 0;
	virtual void SetParametersStatic() = 0;
	virtual void SetParametersDynamic() = 0;

	std::string vertex_file_path;
	std::string fragment_file_path;
	GLuint shader_id;
};

#endif