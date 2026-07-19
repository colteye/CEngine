#ifndef MODEL_IMPORTER_H
#define MODEL_IMPORTER_H

#include "model.h"
#include <string>
#include <vector>

#include <glm/glm.hpp>

class ModelImporter
{
public:
	static Model ImportOBJ(const std::string& model_p,
		const std::unordered_map<std::string, Material*>& in_mats);
};
#endif
