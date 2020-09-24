#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdio.h>
#include "controls.h"

Controls::Controls(GLFWwindow* window)
{
	//glfwSetMouseButtonCallback(window, Controls::mouse_button_callback);
	glfw_window = window;
}

void Controls::Update(Camera* cam, float& deltaTime)
{
	glm::vec3 position = cam->GetPosition();
	glm::vec2 angles = cam->GetAngles();

	float speed = 5.0f; // 3 units / second
	float mouseSpeed = 0.3f;
	double xpos, ypos;
	glfwGetCursorPos(glfw_window, &xpos, &ypos);
	glfwSetCursorPos(glfw_window, 1024 / 2, 768 / 2);

	angles.x += mouseSpeed * float(1024 / 2 - xpos) * deltaTime;
	angles.y += mouseSpeed * float(768 / 2 - ypos) * deltaTime;

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

	float sprint_multiplier = 3.0f;

	float sprint = 1.0f;
	if (glfwGetKey(glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) sprint = sprint_multiplier;
	// Move forward
	if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) {
		position += direction * speed * deltaTime * sprint;
	}
	// Move backward
	if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) {
		position -= direction * speed * deltaTime * sprint;
	}
	// Strafe right
	if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) {
		position += right * speed * deltaTime * sprint;
	}
	// Strafe left
	if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) {
		position -= right * speed * deltaTime * sprint;
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