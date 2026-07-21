#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>

#include <glm/glm.hpp>

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

class Material
{
public:
	Material() = default;
	Material(MaterialShaderType in_shader_type, const std::string& albedo_p, const std::string& normal_p,
		const std::string& metallic_roughness_ao_p);
	Material(Material&&) noexcept = default;
	Material& operator=(Material&&) noexcept = default;
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
	const glm::vec3& GetMetallicRoughnessAoFactors() const { return metallic_roughness_ao_factors; }
	void SetMetallicRoughnessAoFactors(const glm::vec3& factors) { metallic_roughness_ao_factors = factors; }

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
	glm::vec3 metallic_roughness_ao_factors = glm::vec3(0.0f, 0.5f, 1.0f);
	float alpha_cutoff = 0.5f;
	bool receives_shadows = true;
	bool casts_shadows = true;
};

} // namespace CEngine::Renderer

#endif
