#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "shaders/shader.h"
#include "shaders/pbr_standard.h"
#include "texture.h"
#include "model.h"
#include "model_importer.h"
#include "controls.h"
#include "camera.h"
#include "render_system.h"


#include "glm/gtx/string_cast.hpp"
#include "shaders/shader_constants.h"
#include "Main.h"

GLFWwindow* Initialization()
{
	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 + GLSL 150
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
// GL 3.0 + GLSL 130
const char* glsl_version = "#version 130";
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

if (!glfwInit())
{
	fprintf(stderr, "Failed to initialize GLFW\n");
	return nullptr;
}

glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

// Open a window and create its OpenGL context
GLFWwindow* window; // (In the accompanying source code, this variable is global for simplicity)
window = glfwCreateWindow(1024, 768, "Colteh Engine", nullptr, nullptr);
if (window == nullptr) {
	fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
	glfwTerminate();
	return nullptr;
}

// Make the window's context current.
glfwMakeContextCurrent(window);
glfwSwapInterval(1);

// Open GLAD loader.
if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
{
	std::cout << "Failed to initialize OpenGL context" << std::endl;
	return nullptr;
}
return window;
}

int main()
{
	// Init control related stuff.
	GLFWwindow* window = Initialization();
	if (window == nullptr) return -1;

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	Controls control = Controls(window);

	// Start with camera.
	Camera view_cam;

	// Init lighting.
	Light light_1 = Light(glm::vec3(0.0f, 10.0f, 7.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1);
	Light light_4 = Light(glm::vec3(0.0f, 7.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 5);

	// Init shaders after.
	PBRStandard pbr_shader;

	// Init materials after shaders.
	Material barrelMaterial = Material(&pbr_shader,
		"test_model/results/barrel_albedo.DDS",
		"test_model/results/barrel_normal.DDS",
		"test_model/results/barrel_metallic_roughness_ao.DDS");

	Material floorMaterial = Material(&pbr_shader,
		"missing/missing.DDS",
		"test_model/results/barrel_normal.DDS",
		"test_model/results/barrel_metallic_roughness_ao.DDS");

	// Init models after materials.
	Model barrel_x = ModelImporter::ImportOBJ("test_model/barrel_low.obj", { {"barrel", &barrelMaterial } });
	Model barrel_y = ModelImporter::ImportOBJ("test_model/barrel_low2.obj", { {"barrel", &barrelMaterial } });
	Model floor = ModelImporter::ImportOBJ("test_model/barrel_floor.obj", { {"None", &floorMaterial } });

	Model sphere = ModelImporter::ImportOBJ("test_model/hard_sphere.obj", { {"Mat1", &floorMaterial }, {"Mat2", &barrelMaterial } });

	// Testing
	//glm::vec3 start_pos = glm::vec3(0.0f, 10.0f, 7.0f);

	// Time to initialize renderer!
	int window_width, window_height;
	glfwGetFramebufferSize(window, &window_width, &window_height);
	RenderSystem::Initialize(window_width, window_height);

	double lastTime = glfwGetTime();
	do {
		double current = glfwGetTime();
		float deltaTime = float(current - lastTime);
		lastTime = current;

		control.Update(&view_cam, deltaTime);

		//start_pos = glm::vec3{ start_pos.x, start_pos.y + 0.08f, start_pos.z };
		//light_4.setPosition(start_pos);

		RenderSystem::Render();

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);
}