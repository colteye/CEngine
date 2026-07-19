#include "light.h"
#include "render_system.h"

Light::Light(const glm::vec3& pos, const glm::vec3& col, float pow)
	: m_id(RenderSystem::RegisterLight(pos, col, pow)),
	  m_position(pos),
	  m_color(col),
	  m_power(pow)
{
}

void Light::SetPosition(const glm::vec3& pos)
{
	RenderSystem::UpdateLight(m_id, pos, m_color, m_power);
	m_position = pos;
}

void Light::SetColor(const glm::vec3& col)
{
	RenderSystem::UpdateLight(m_id, m_position, col, m_power);
	m_color = col;
}

void Light::SetPower(float pow)
{
	RenderSystem::UpdateLight(m_id, m_position, m_color, pow);
	m_power = pow;
}
