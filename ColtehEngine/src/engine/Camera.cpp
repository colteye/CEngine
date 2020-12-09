	#include <glm/gtc/matrix_transform.hpp>

	#include <stdio.h>

	#include "shaders/shader_constants.h"
	#include "Camera.h"

	void Camera::SetAnglesCartesian(glm::vec3 ang)
	{
		angles = glm::vec2{ atan(ang.x/ang.y), 
							atan(sqrt( (ang.x * ang.x) + (ang.y * ang.y) ) / ang.z) };
	}

	void Camera::UpdateMatrices()
	{
		// Direction : Spherical coordinates to Cartesian coordinates conversion
		glm::vec3 direction(
			cos(angles.y) * sin(angles.x),
			sin(angles.y),
			cos(angles.y) * cos(angles.x)
		);

		// Right vector
		glm::vec3 right = glm::vec3(
			sin(angles.x - glm::pi<float>() / 2.0f),
			0,
			cos(angles.x - glm::pi<float>() / 2.0f)
		);

		// Up vector : perpendicular to both direction and right
		glm::vec3 up = glm::cross(right, direction);

		// Projection matrix : 45&deg; Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
		proj = glm::perspective(glm::radians(field_of_view), 4.0f / 3.0f, 0.1f, 100.0f);
		// Camera matrix
		view = glm::lookAt(
			position,           // Camera is here
			position + direction, // and looks here : at the same position, plus "direction"
			up                  // Head is up (set to 0,-1,0 to look upside-down)
		);

		model = glm::mat4(1.0f);

		ShaderConstants::model = model;
		ShaderConstants::view = view;
		ShaderConstants::proj = proj;
		ShaderConstants::camera_position = position;
	}