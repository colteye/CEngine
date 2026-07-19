#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <glm/glm.hpp>

class Light
{
public:
	Light(const glm::vec3& pos, const glm::vec3& col, float pow);

	void SetPosition(const glm::vec3& pos);
	void SetColor(const glm::vec3& col);
	void SetPower(float pow);

	const glm::vec3& GetPosition() const { return m_position; };
	const glm::vec3& GetColor() const { return m_color; };
	float GetPower() const { return m_power; };

private:
	// Have to keep track of an ID as the actual data is stored in the render system.
	unsigned int m_id = 0;
	glm::vec3 m_position = glm::vec3(0.0f);
	glm::vec3 m_color = glm::vec3(1.0f);
	float m_power = 1.0f;
};
#endif
