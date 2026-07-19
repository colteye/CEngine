#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>

enum class MaterialShaderType
{
	PBRStandard
};

class Material
{
public:
	Material(MaterialShaderType in_shader_type, const std::string& albedo_p, const std::string& normal_p,
		const std::string& metallic_roughness_ao_p);
	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;
	~Material() = default;

	const std::string& GetAlbedoPath() const { return albedo_path; }
	const std::string& GetNormalPath() const { return normal_path; }
	const std::string& GetMetallicRoughnessAoPath() const { return metallic_roughness_ao_path; }

	std::string material_name;
	MaterialShaderType shader_type = MaterialShaderType::PBRStandard;

private:
	std::string albedo_path;
	std::string normal_path;
	std::string metallic_roughness_ao_path;
};
#endif
