#ifndef CONTROLS_H
#define CONTROLS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "camera.h"

class Controls
{
public:
	Controls(GLFWwindow* window);
	void Update(Camera* cam, float& deltaTime);
	//glm::mat4 ComputeMVPFromInput(float& deltaTime);
	//glm::mat4 GetModelMatrix() { return model; }
	//glm::mat4 GetViewMatrix() { return view; }
	//glm::mat4 GetProjectionMatrix() { return proj; }
private:
	GLFWwindow* glfw_window;
	//glm::vec3 position = glm::vec3(0, 0, 5);
	//glm::vec2 angles = glm::vec2(3.14f, 0.0f);

	//glm::mat4 model;
	//glm::mat4 view;
	//glm::mat4 proj;

	//float fov = 45.0f;

	//static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
};

#endif