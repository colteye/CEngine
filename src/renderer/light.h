#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace CEngine::Renderer {

using LightHandle = uint32_t;

enum class LightType
{
	Directional = 0,
	Point = 1,
	Spot = 2
};

struct LightRecord
{
	LightType type = LightType::Point;
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float range = 0.0f;
	float spot_inner_cos = 0.0f;
	float spot_outer_cos = 0.0f;
	bool enabled = true;
	bool casts_shadows = false;
	uint32_t shadow_resolution = 1024;
	uint32_t shadow_update_rate = 1;
	float shadow_bias = 0.0015f;
	float shadow_normal_bias = 0.02f;
};

struct GpuLight
{
	glm::vec4 position_range;
	glm::vec4 direction_spot;
	glm::vec4 color_intensity;
	glm::vec4 params; // x: type, y: spot inner cos, z: shadow index, w: shadow type
};

struct LightShadowGpuHandle
{
	int32_t index = -1;
	int32_t type = 0;

	bool operator==(const LightShadowGpuHandle& other) const
	{
		return index == other.index && type == other.type;
	}
};

class Light
{
public:
	Light(const glm::vec3& pos, const glm::vec3& col, float pow);

	void SetPosition(const glm::vec3& pos);
	void SetDirection(const glm::vec3& dir);
	void SetColor(const glm::vec3& col);
	void SetPower(float pow);
	void SetIntensity(float intensity);
	void SetRange(float range);
	void SetSpotAngles(float inner_degrees, float outer_degrees);
	void SetCastsShadows(bool casts);
	void SetShadowResolution(uint32_t resolution);
	void SetShadowUpdateRate(uint32_t frame_interval);
	void SetShadowBias(float bias, float normal_bias);

	const glm::vec3& GetPosition() const { return m_position; };
	const glm::vec3& GetDirection() const { return m_direction; };
	const glm::vec3& GetColor() const { return m_color; };
	float GetPower() const { return m_intensity; };
	float GetIntensity() const { return m_intensity; };
	LightType GetType() const { return m_type; };
	bool CastsShadows() const { return m_casts_shadows; };

protected:
	explicit Light(const LightRecord& record);

private:
	void UpdateRecord();

	// Have to keep track of an ID as the actual data is stored in the render system.
	LightHandle m_id = 0;
	LightType m_type = LightType::Point;
	glm::vec3 m_position = glm::vec3(0.0f);
	glm::vec3 m_direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 m_color = glm::vec3(1.0f);
	float m_intensity = 1.0f;
	float m_range = 0.0f;
	float m_spot_inner_cos = 0.0f;
	float m_spot_outer_cos = 0.0f;
	bool m_enabled = true;
	bool m_casts_shadows = false;
	uint32_t m_shadow_resolution = 1024;
	uint32_t m_shadow_update_rate = 1;
	float m_shadow_bias = 0.0015f;
	float m_shadow_normal_bias = 0.02f;
};

class DirectionalLight final : public Light
{
public:
	DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity);
};

class PointLight final : public Light
{
public:
	PointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range = 0.0f);
};

class SpotLight final : public Light
{
public:
	SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
		float intensity, float inner_degrees, float outer_degrees, float range = 0.0f);
};

} // namespace CEngine::Renderer

#endif
