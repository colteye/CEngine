#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <unordered_map>
#include "material.h"
#include "mesh.h"
#include "renderable.h"

class Model
{
public:
	std::string model_name;
	Model(std::unordered_map<std::string, Mesh> in_meshes,
		std::unordered_map<std::string, Material*> in_mats, bool auto_register = true);
	std::vector<RenderableHandle> RegisterRenderables(const glm::mat4& transform,
		uint32_t flags = RenderableFlagStatic | RenderableFlagCastsShadow | RenderableFlagReceivesShadow) const;

private:
	std::unordered_map<std::string, Mesh> meshes;
	std::unordered_map<std::string, Material*> mats;
};
#endif
