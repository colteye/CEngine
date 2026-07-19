#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

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
};

struct GpuLight
{
	glm::vec4 position_range;
	glm::vec4 direction_spot;
	glm::vec4 color_intensity;
	glm::vec4 params;
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

	const glm::vec3& GetPosition() const { return m_position; };
	const glm::vec3& GetDirection() const { return m_direction; };
	const glm::vec3& GetColor() const { return m_color; };
	float GetPower() const { return m_intensity; };
	float GetIntensity() const { return m_intensity; };
	LightType GetType() const { return m_type; };

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
#endif
