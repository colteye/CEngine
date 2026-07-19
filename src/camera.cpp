#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdio.h>

#include "shaders/shader_constants.h"
#include "camera.h"

Camera::Camera()
{
	UpdateMatrices();
}

void Camera::SetAnglesCartesian(const glm::vec3& ang)
{
	m_angles = glm::vec2{
		std::atan2(ang.x, ang.y),
		std::atan2(std::sqrt((ang.x * ang.x) + (ang.y * ang.y)), ang.z)
	};
	UpdateMatrices();
}

void Camera::UpdateMatrices()
{
	// Direction : Spherical coordinates to Cartesian coordinates conversion
	glm::vec3 direction(
		std::cos(m_angles.y) * std::sin(m_angles.x),
		std::sin(m_angles.y),
		std::cos(m_angles.y) * std::cos(m_angles.x)
	);

	// Right vector
	glm::vec3 right = glm::vec3(
		std::sin(m_angles.x - glm::pi<float>() / 2.0f),
		0,
		std::cos(m_angles.x - glm::pi<float>() / 2.0f)
	);

	// Up vector : perpendicular to both direction and right
	glm::vec3 up = glm::cross(right, direction);

	// Projection matrix : 45 degree Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	m_projection = glm::perspective(glm::radians(m_field_of_view), 4.0f / 3.0f, 0.1f, 100.0f);
	// Camera matrix
	m_view = glm::lookAt(
		m_position,           // Camera is here
		m_position + direction, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);

	m_model = glm::mat4(1.0f);

	ShaderConstants::model = m_model;
	ShaderConstants::view = m_view;
	ShaderConstants::proj = m_projection;
	ShaderConstants::camera_position = m_position;
}
