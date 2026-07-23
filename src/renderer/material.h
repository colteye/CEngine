#ifndef MATERIAL_H
#define MATERIAL_H

#include "renderer/texture.h"

#include <glm/glm.hpp>

#include <string>

namespace CEngine::Renderer {

enum class MaterialShaderType : unsigned int
{
	Unknown = 0,
	PBRStandard = 1,
	Unlit = 2,
};

enum class MaterialRenderMode
{
	OpaqueDeferred,
	AlphaClip,
	AlphaHashDither,
	TransparentBlend,
	Unlit
};

constexpr bool RequiresAlphaTest(MaterialRenderMode mode)
{
	return mode == MaterialRenderMode::AlphaClip ||
		mode == MaterialRenderMode::AlphaHashDither;
}

struct Material {
	std::string material_name;
	MaterialShaderType shader_type = MaterialShaderType::PBRStandard;
	Texture albedo;
	Texture normal;
	Texture metallic_roughness_ao;
	MaterialRenderMode render_mode = MaterialRenderMode::OpaqueDeferred;
	glm::vec4 base_color_factor = glm::vec4(1.0f);
	glm::vec3 metallic_roughness_ao_factors = glm::vec3(0.0f, 0.5f, 1.0f);
	float alpha_cutoff = 0.5f;
	bool receives_shadows = true;
	bool casts_shadows = true;
};

} // namespace CEngine::Renderer

#endif
