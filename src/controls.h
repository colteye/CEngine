#ifndef CONTROLS_H
#define CONTROLS_H

#include <GLFW/glfw3.h>
#include "camera.h"

class Controls
{
public:
	Controls(GLFWwindow* window);
	void Update(Camera* cam, float delta_time);
private:
	GLFWwindow* glfw_window = nullptr;
	double previous_cursor_x = 0.0;
	double previous_cursor_y = 0.0;
	bool has_cursor_sample = false;
	bool was_focused = false;
};

#endif
