#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Light
{
public:
	Light(glm::vec3 light_pos, glm::vec3 light_col, float pow);

	// Have to keep track of an ID as the actual data is stored in the render system.
	unsigned int id;
};
#endif