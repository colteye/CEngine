#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "model.h"
#include "model_importer.h"
#include "controls.h"
#include "camera.h"
#include "render_system.h"


#include "glm/gtx/string_cast.hpp"
#include "shader_constants.h"
#include "main.h"

namespace {
RenderBackendType ParseBackend(int argc, char** argv)
{
	for (int index = 1; index < argc; ++index)
	{
		if (std::strcmp(argv[index], "--vulkan") == 0)
		{
			return RenderBackendType::Vulkan;
		}
		if (std::strcmp(argv[index], "--opengl") == 0)
		{
			return RenderBackendType::OpenGL;
		}
		if (std::strcmp(argv[index], "--backend") == 0 && index + 1 < argc)
		{
			++index;
			if (std::strcmp(argv[index], "vulkan") == 0)
			{
				return RenderBackendType::Vulkan;
			}
			if (std::strcmp(argv[index], "opengl") == 0)
			{
				return RenderBackendType::OpenGL;
			}
		}
	}

	return RenderBackendType::OpenGL;
}
} // namespace

GLFWwindow* Initialization(RenderBackendType backend_type)
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return nullptr;
	}

	if (backend_type == RenderBackendType::Vulkan)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		GLFWwindow* window = glfwCreateWindow(1024, 768, "Colteh Engine - Vulkan", nullptr, nullptr);
		if (window == nullptr)
		{
			fprintf(stderr, "Failed to open GLFW window for Vulkan.\n");
			glfwTerminate();
			return nullptr;
		}
		return window;
	}

	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 + GLSL 150
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
	// GL 3.0 + GLSL 130
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

	// Open a window and create its OpenGL context
	GLFWwindow* window = glfwCreateWindow(1024, 768, "Colteh Engine", nullptr, nullptr);
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

int main(int argc, char** argv)
{
	const RenderBackendType backend_type = ParseBackend(argc, argv);

	// Init control related stuff.
	GLFWwindow* window = Initialization(backend_type);
	if (window == nullptr) return -1;

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	// Time to initialize renderer!
	int window_width, window_height;
	glfwGetFramebufferSize(window, &window_width, &window_height);
	if (!RenderSystem::Initialize(backend_type, window, window_width, window_height))
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	{
		Controls control = Controls(window);

		// Start with camera.
		Camera view_cam;

		// Init lighting.
		[[maybe_unused]] Light light_1 = Light(glm::vec3(0.0f, 10.0f, 7.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1);
		[[maybe_unused]] Light light_4 = Light(glm::vec3(0.0f, 7.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 5);

		// Init materials after the render backend.
		Material barrelMaterial = Material(MaterialShaderType::PBRStandard,
			"assets/demo/barrel/results/barrel_albedo.DDS",
			"assets/demo/barrel/results/barrel_normal.DDS",
			"assets/demo/barrel/results/barrel_metallic_roughness_ao.DDS");

		// Init models after materials.
		Model barrel = ModelImporter::ImportOBJ("assets/demo/barrel/barrel_low.obj", { {"barrel", &barrelMaterial } });

		// Testing
		//glm::vec3 start_pos = glm::vec3(0.0f, 10.0f, 7.0f);

		double lastTime = glfwGetTime();
		do {
			double current = glfwGetTime();
			float deltaTime = float(current - lastTime);
			lastTime = current;

			control.Update(&view_cam, deltaTime);

			//start_pos = glm::vec3{ start_pos.x, start_pos.y + 0.08f, start_pos.z };
			//light_4.SetPosition(start_pos);

			RenderSystem::Render();

			// Swap buffers
			if (backend_type == RenderBackendType::OpenGL)
			{
				glfwSwapBuffers(window);
			}
			glfwPollEvents();
		} // Check if the ESC key was pressed or the window was closed
		while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
			glfwWindowShouldClose(window) == 0);

		RenderSystem::Shutdown();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
