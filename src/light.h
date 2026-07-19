#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Light
{
public:
	Light(glm::vec3 pos, glm::vec3 col, float pow);

	void setPosition(glm::vec3 pos);
	void setColor(glm::vec3 col);
	void setPower(float pow);

	glm::vec3 getPosition() { return l_pos; };
	glm::vec3 getColor() { return l_col; };
	float getPower() { return l_pow; };

	// Have to keep track of an ID as the actual data is stored in the render system.
	unsigned int l_id;
	glm::vec3 l_pos;
	glm::vec3 l_col;
	float l_pow;
};
#endif