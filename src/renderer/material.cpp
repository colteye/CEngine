#include "renderer/material.h"

namespace CEngine::Renderer {

Material::Material(MaterialShaderType in_shader_type, const std::string& albedo_p, const std::string& normal_p,
	const std::string& metallic_roughness_ao_p)
	: shader_type(in_shader_type),
	  albedo_path(albedo_p),
	  normal_path(normal_p),
	  metallic_roughness_ao_path(metallic_roughness_ao_p)
{
}

} // namespace CEngine::Renderer
