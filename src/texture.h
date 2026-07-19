#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <glad/glad.h>

class Texture
{
public:
	static GLuint LoadDDS(const std::string& imagepath);
};
#endif
