#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Camera
{
public:
	Camera();

	const glm::vec3& GetPosition() const { return m_position; };
	void SetPosition(const glm::vec3& pos) { m_position = pos; UpdateMatrices(); };

	const glm::vec2& GetAngles() const { return m_angles; };
	void SetAnglesCartesian(const glm::vec3& ang);
	void SetAngles(const glm::vec2& ang) { m_angles = ang; UpdateMatrices(); };

	void SetPositionAngles(const glm::vec3& pos, const glm::vec2& ang) { m_position = pos; m_angles = ang; UpdateMatrices(); };

	float GetFOV() const { return m_field_of_view; };
	void SetFOV(float fov) { m_field_of_view = fov; UpdateMatrices(); };
	
private:

	void UpdateMatrices();
	glm::vec3 m_position = glm::vec3(0, 0, 5);
	glm::vec2 m_angles = glm::vec2(3.14f, 0.0f);

	glm::mat4 m_model;
	glm::mat4 m_view;
	glm::mat4 m_projection;

	float m_field_of_view = 45.0f;
};

#endif
