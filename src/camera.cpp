#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <stdio.h>

#include "renderer/render_system.h"
#include "camera.h"

namespace Renderer = CEngine::Renderer;

Camera::Camera()
{
	UpdateMatrices();
}

void Camera::SetAnglesCartesian(const glm::vec3& ang)
{
	const glm::vec3 direction = glm::normalize(ang);
	m_angles = glm::vec2{
		std::atan2(direction.y, direction.x),
		std::asin(std::clamp(direction.z, -1.0f, 1.0f))
	};
	UpdateMatrices();
}

void Camera::SetAspectRatio(float aspect_ratio)
{
	if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f)
	{
		return;
	}
	m_aspect_ratio = aspect_ratio;
	UpdateMatrices();
}

void Camera::UpdateMatrices()
{
	// Direction : Spherical coordinates to Cartesian coordinates conversion
	const glm::vec3 direction(
		std::cos(m_angles.y) * std::cos(m_angles.x),
		std::cos(m_angles.y) * std::sin(m_angles.x),
		std::sin(m_angles.y));

	// Right vector
	const glm::vec3 right = glm::normalize(
		glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f)));

	// Up vector : perpendicular to both direction and right
	const glm::vec3 up = glm::cross(right, direction);

	// Projection matrix uses the current framebuffer aspect ratio.
	m_projection = glm::perspective(glm::radians(m_field_of_view), m_aspect_ratio, 0.1f, 100.0f);
	// Camera matrix
	m_view = glm::lookAt(
		m_position,           // Camera is here
		m_position + direction, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);

	m_model = glm::mat4(1.0f);

	Renderer::RenderFrameConstants constants;
	constants.model = m_model;
	constants.view = m_view;
	constants.proj = m_projection;
	constants.camera_position = m_position;
	Renderer::RenderSystem::SetFrameConstants(constants);
}
