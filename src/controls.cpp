#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <stdio.h>
#include "controls.h"

Controls::Controls(GLFWwindow* window)
{
	glfw_window = window;
	if (glfw_window == nullptr) return;
	glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported() == GLFW_TRUE)
		glfwSetInputMode(glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Controls::Update(Camera* cam, float delta_time)
{
	glm::vec3 position = cam->GetPosition();
	glm::vec2 angles = cam->GetAngles();

	const float speed = 5.0f;
	const float mouse_sensitivity = 0.003f;
	const bool focused = glfwGetWindowAttrib(glfw_window, GLFW_FOCUSED) == GLFW_TRUE;
	double cursor_x = 0.0;
	double cursor_y = 0.0;
	glfwGetCursorPos(glfw_window, &cursor_x, &cursor_y);
	if (!focused || !was_focused || !has_cursor_sample)
	{
		previous_cursor_x = cursor_x;
		previous_cursor_y = cursor_y;
		has_cursor_sample = focused;
	}
	else
	{
		const float mouse_x = static_cast<float>(cursor_x - previous_cursor_x);
		const float mouse_y = static_cast<float>(cursor_y - previous_cursor_y);
		angles.x -= mouse_sensitivity * mouse_x;
		angles.y -= mouse_sensitivity * mouse_y;
		previous_cursor_x = cursor_x;
		previous_cursor_y = cursor_y;
	}
	was_focused = focused;
	angles.y = std::clamp(angles.y, -1.55f, 1.55f);

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	const glm::vec3 direction(
		cos(angles.y) * cos(angles.x),
		cos(angles.y) * sin(angles.x),
		sin(angles.y));

	// Right vector
	const glm::vec3 right = glm::normalize(
		glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f)));


	const float sprint_multiplier = 3.0f;

	float sprint = 1.0f;
	if (glfwGetKey(glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) sprint = sprint_multiplier;
	// Move forward
	if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) {
		position += direction * speed * delta_time * sprint;
	}
	// Move backward
	if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) {
		position -= direction * speed * delta_time * sprint;
	}
	// Strafe right
	if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) {
		position += right * speed * delta_time * sprint;
	}
	// Strafe left
	if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) {
		position -= right * speed * delta_time * sprint;
	}

	cam->SetPositionAngles(position, angles);
}
