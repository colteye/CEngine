#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <glad/glad.h>

class Texture
{
public:
	static GLuint LoadDDS(std::string imagepath);
};
#endif