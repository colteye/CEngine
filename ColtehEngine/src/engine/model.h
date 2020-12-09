#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class Model
{
public:
	Model(std::vector<glm::vec3>& in_vertices,
		std::vector<glm::vec2>& in_uvs,
		std::vector<glm::vec3>& in_normals,
		std::vector<glm::vec3>& in_tangents);

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
};
#endif