#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class Model
{
public:
	static bool LoadOBJ(std::string modelpath, 
						std::vector<glm::vec3>& out_vertices,
						std::vector<glm::vec2>& out_uvs,
						std::vector<glm::vec3>& out_normals);

};
#endif