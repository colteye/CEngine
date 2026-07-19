#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include "material.h"
#include "mesh.h"

class Model
{
public:
	std::string model_name;
	Model(std::unordered_map<std::string, Mesh>& in_meshes, std::unordered_map<std::string, Material*>& in_mats);

private:
	std::unordered_map<std::string, Mesh> meshes;
	std::unordered_map<std::string, Material*> mats;
};
#endif