
#include "material.h"
//#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include "render_system.h"
#include "texture.h"

Material::Material(PBRStandard* in_shader, const std::string& albedo_p, const std::string& normal_p,
	const std::string& metallic_roughness_ao_p)
	: albedo_tex(0),
	  normal_tex(0),
	  metallic_roughness_ao_tex(0),
	  shader(in_shader)
{
	albedo_tex = Texture::LoadDDS(albedo_p);
	normal_tex = Texture::LoadDDS(normal_p);
	metallic_roughness_ao_tex = Texture::LoadDDS(metallic_roughness_ao_p);

	RenderSystem::RegisterMaterial(this);
}

Material::~Material()
{
	if (albedo_tex != 0)
	{
		glDeleteTextures(1, &albedo_tex);
		albedo_tex = 0;
	}
	if (normal_tex != 0)
	{
		glDeleteTextures(1, &normal_tex);
		normal_tex = 0;
	}
	if (metallic_roughness_ao_tex != 0)
	{
		glDeleteTextures(1, &metallic_roughness_ao_tex);
		metallic_roughness_ao_tex = 0;
	}
}

void Material::UpdateShader()
{
	shader->SetTextures(albedo_tex, normal_tex, metallic_roughness_ao_tex);
	shader->Update();
}
