#ifndef SHADER_CONSTANTS_H
#define SHADER_CONSTANTS_H

#include <glm/glm.hpp>

class ShaderConstants
{
public:
	static glm::vec3 camera_position;
	static glm::mat4 model;
	static glm::mat4 view;
	static glm::mat4 proj;
};

#endif