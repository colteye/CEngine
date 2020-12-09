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
private:
	GLFWwindow* glfw_window;
};

#endif