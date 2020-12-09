#ifndef MODEL_IMPORTER_H
#define MODEL_IMPORTER_H

#include "model.h"
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class ModelImporter
{
public:
	static Model ImportOBJ(std::string model_p);
};
#endif