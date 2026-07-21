#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <stdio.h>
#include "controls.h"

Controls::Controls(GLFWwindow* window)
{
	//glfwSetMouseButtonCallback(window, Controls::mouse_button_callback);
	glfw_window = window;
}

void Controls::Update(Camera* cam, float delta_time)
{
	glm::vec3 position = cam->GetPosition();
	glm::vec2 angles = cam->GetAngles();

	float speed = 5.0f; // 3 units / second
	float mouseSpeed = 0.3f;
	double xpos, ypos;
	int window_width = 0;
	int window_height = 0;
	glfwGetWindowSize(glfw_window, &window_width, &window_height);

	const double center_x = window_width * 0.5;
	const double center_y = window_height * 0.5;
	glfwGetCursorPos(glfw_window, &xpos, &ypos);
	glfwSetCursorPos(glfw_window, center_x, center_y);

	angles.x += mouseSpeed * static_cast<float>(center_x - xpos) * delta_time;
	angles.y += mouseSpeed * static_cast<float>(center_y - ypos) * delta_time;
	angles.y = std::clamp(angles.y, -1.55f, 1.55f);

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	const glm::vec3 direction(
		cos(angles.y) * cos(angles.x),
		cos(angles.y) * sin(angles.x),
		sin(angles.y));

	// Right vector
	const glm::vec3 right = glm::normalize(
		glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f)));


	float sprint_multiplier = 3.0f;

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

/*void Controls::mouse_button_callback(GLFWwindow * window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		// [...]
	}
}*/
