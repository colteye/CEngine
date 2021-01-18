
#include "Material.h"
//#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include "render_system.h"
#include "texture.h"

Material::Material(PBRStandard *in_shader, std::string albedo_p, std::string normal_p, std::string metallic_roughness_ao_p)
{
	shader = in_shader;
	albedo_tex = Texture::LoadDDS(albedo_p);
	normal_tex = Texture::LoadDDS(normal_p);
	metallic_roughness_ao_tex = Texture::LoadDDS(metallic_roughness_ao_p);

	RenderSystem::RegisterMaterial(this);
}

void Material::UpdateShader()
{
	shader->SetTextures(albedo_tex, normal_tex, metallic_roughness_ao_tex);
	shader->SetParameters();
}
