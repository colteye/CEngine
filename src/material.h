#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>

#include <glm/glm.hpp>

enum class MaterialShaderType
{
	PBRStandard
};

enum class MaterialRenderMode
{
	OpaqueDeferred,
	AlphaClip,
	AlphaHashDither,
	TransparentBlend,
	Unlit
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

	MaterialRenderMode GetRenderMode() const { return render_mode; }
	void SetRenderMode(MaterialRenderMode mode) { render_mode = mode; }

	float GetAlphaCutoff() const { return alpha_cutoff; }
	void SetAlphaCutoff(float cutoff) { alpha_cutoff = cutoff; }

	const glm::vec4& GetBaseColorFactor() const { return base_color_factor; }
	void SetBaseColorFactor(const glm::vec4& color) { base_color_factor = color; }

	bool ReceivesShadows() const { return receives_shadows; }
	void SetReceivesShadows(bool receives) { receives_shadows = receives; }

	bool CastsShadows() const { return casts_shadows; }
	void SetCastsShadows(bool casts) { casts_shadows = casts; }

	std::string material_name;
	MaterialShaderType shader_type = MaterialShaderType::PBRStandard;

private:
	std::string albedo_path;
	std::string normal_path;
	std::string metallic_roughness_ao_path;
	MaterialRenderMode render_mode = MaterialRenderMode::OpaqueDeferred;
	glm::vec4 base_color_factor = glm::vec4(1.0f);
	float alpha_cutoff = 0.5f;
	bool receives_shadows = true;
	bool casts_shadows = true;
};
#endif
