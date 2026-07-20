#include "renderer/light.h"
#include "renderer/render_system.h"

#include <cmath>
#include <algorithm>

namespace {
glm::vec3 NormalizeOrDefault(const glm::vec3& value, const glm::vec3& fallback)
{
	const float length = glm::length(value);
	if (length <= 0.00001f)
	{
		return fallback;
	}
	return value / length;
}

float DegreesToCos(float degrees)
{
	return std::cos(glm::radians(degrees));
}
} // namespace

namespace CEngine::Renderer {

Light::Light(const glm::vec3& pos, const glm::vec3& col, float pow)
	: Light(LightRecord {
		LightType::Point,
		pos,
		glm::vec3(0.0f, -1.0f, 0.0f),
		col,
		pow,
		0.0f,
		0.0f,
		0.0f,
		true
	})
{
}

Light::Light(const LightRecord& record)
	: m_id(RenderSystem::RegisterLight(record)),
	  m_type(record.type),
	  m_position(record.position),
	  m_direction(NormalizeOrDefault(record.direction, glm::vec3(0.0f, -1.0f, 0.0f))),
	  m_color(record.color),
	  m_intensity(record.intensity),
	  m_range(record.range),
	  m_spot_inner_cos(record.spot_inner_cos),
	  m_spot_outer_cos(record.spot_outer_cos),
	  m_enabled(record.enabled),
	  m_casts_shadows(record.casts_shadows),
	  m_shadow_resolution(record.shadow_resolution),
	  m_shadow_update_rate(record.shadow_update_rate),
	  m_shadow_bias(record.shadow_bias),
	  m_shadow_normal_bias(record.shadow_normal_bias)
{
}

void Light::SetPosition(const glm::vec3& pos)
{
	m_position = pos;
	UpdateRecord();
}

void Light::SetDirection(const glm::vec3& dir)
{
	m_direction = NormalizeOrDefault(dir, m_direction);
	UpdateRecord();
}

void Light::SetColor(const glm::vec3& col)
{
	m_color = col;
	UpdateRecord();
}

void Light::SetPower(float pow)
{
	SetIntensity(pow);
}

void Light::SetIntensity(float intensity)
{
	m_intensity = intensity;
	UpdateRecord();
}

void Light::SetRange(float range)
{
	m_range = range;
	UpdateRecord();
}

void Light::SetSpotAngles(float inner_degrees, float outer_degrees)
{
	m_spot_inner_cos = DegreesToCos(inner_degrees);
	m_spot_outer_cos = DegreesToCos(outer_degrees);
	UpdateRecord();
}

void Light::SetCastsShadows(bool casts)
{
	m_casts_shadows = casts;
	UpdateRecord();
}

void Light::SetShadowResolution(uint32_t resolution)
{
	m_shadow_resolution = std::max<uint32_t>(64, resolution);
	UpdateRecord();
}

void Light::SetShadowUpdateRate(uint32_t frame_interval)
{
	m_shadow_update_rate = std::max<uint32_t>(1, frame_interval);
	UpdateRecord();
}

void Light::SetShadowBias(float bias, float normal_bias)
{
	m_shadow_bias = std::max(0.0f, bias);
	m_shadow_normal_bias = std::max(0.0f, normal_bias);
	UpdateRecord();
}

void Light::UpdateRecord()
{
	LightRecord record;
	record.type = m_type;
	record.position = m_position;
	record.direction = m_direction;
	record.color = m_color;
	record.intensity = m_intensity;
	record.range = m_range;
	record.spot_inner_cos = m_spot_inner_cos;
	record.spot_outer_cos = m_spot_outer_cos;
	record.enabled = m_enabled;
	record.casts_shadows = m_casts_shadows;
	record.shadow_resolution = m_shadow_resolution;
	record.shadow_update_rate = m_shadow_update_rate;
	record.shadow_bias = m_shadow_bias;
	record.shadow_normal_bias = m_shadow_normal_bias;
	RenderSystem::UpdateLight(m_id, record);
}

DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
	: Light(LightRecord {
		LightType::Directional,
		glm::vec3(0.0f),
		NormalizeOrDefault(direction, glm::vec3(0.0f, -1.0f, 0.0f)),
		color,
		intensity,
		0.0f,
		0.0f,
		0.0f,
		true
	})
{
}

PointLight::PointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
	: Light(LightRecord {
		LightType::Point,
		position,
		glm::vec3(0.0f, -1.0f, 0.0f),
		color,
		intensity,
		range,
		0.0f,
		0.0f,
		true
	})
{
}

SpotLight::SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
	float intensity, float inner_degrees, float outer_degrees, float range)
	: Light(LightRecord {
		LightType::Spot,
		position,
		NormalizeOrDefault(direction, glm::vec3(0.0f, -1.0f, 0.0f)),
		color,
		intensity,
		range,
		DegreesToCos(inner_degrees),
		DegreesToCos(outer_degrees),
		true
	})
{
}

} // namespace CEngine::Renderer
