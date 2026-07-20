#include <iostream>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <vector>

#if defined(CENGINE_ENABLE_OPENGL)
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "model.h"
#include "model_importer.h"
#include "controls.h"
#include "camera.h"
#include "physics/physics_system.h"
#include "render_system.h"

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
#include "physics/jolt/jolt_physics_backend.h"
#endif

#include "glm/gtx/string_cast.hpp"
#include "main.h"

namespace {
struct SimulatedModel
{
	PhysicsBodyHandle body = kInvalidPhysicsBodyHandle;
	std::vector<RenderableHandle> renderables;
	glm::mat4 body_to_render = glm::mat4(1.0f);
};

glm::mat4 MakeTransform(const glm::vec3& position, float yaw_degrees, float scale)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
	transform = glm::rotate(transform, glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
	return glm::scale(transform, glm::vec3(scale));
}

glm::quat MakeYawRotation(float yaw_degrees)
{
	return glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
}

MeshData MakeFloorMeshData(float half_extent)
{
	MeshData data;
	data.vertices = {
		glm::vec3(-half_extent, 0.0f, -half_extent),
		glm::vec3(half_extent, 0.0f, half_extent),
		glm::vec3(half_extent, 0.0f, -half_extent),
		glm::vec3(-half_extent, 0.0f, -half_extent),
		glm::vec3(-half_extent, 0.0f, half_extent),
		glm::vec3(half_extent, 0.0f, half_extent),
	};
	data.uvs = {
		glm::vec2(0.0f, 0.0f),
		glm::vec2(6.0f, 6.0f),
		glm::vec2(6.0f, 0.0f),
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 6.0f),
		glm::vec2(6.0f, 6.0f),
	};
	data.normals.assign(data.vertices.size(), glm::vec3(0.0f, 1.0f, 0.0f));
	data.tangents.assign(data.vertices.size(), glm::vec3(1.0f, 0.0f, 0.0f));
	return data;
}

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

#if defined(CENGINE_ENABLE_OPENGL)
	return RenderBackendType::OpenGL;
#elif defined(CENGINE_ENABLE_VULKAN)
	return RenderBackendType::Vulkan;
#else
	return RenderBackendType::OpenGL;
#endif
}

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
SimulatedModel CreateDynamicBarrel(PhysicsSystem& physics, const Model& model,
	const glm::vec3& floor_position, float yaw_degrees, const glm::vec3& initial_velocity)
{
	constexpr float kVisualScale = 1.0f;
	constexpr float kBodyHalfHeight = 0.82f;
	const glm::vec3 body_position = floor_position + glm::vec3(0.0f, kBodyHalfHeight, 0.0f);

	PhysicsBodyDesc body_desc;
	body_desc.motion_type = PhysicsMotionType::Dynamic;
	body_desc.shape_type = PhysicsShapeType::Box;
	body_desc.position = body_position;
	body_desc.rotation = MakeYawRotation(yaw_degrees);
	body_desc.box_half_extents = glm::vec3(0.46f, kBodyHalfHeight, 0.46f);
	body_desc.mass = 18.0f;
	body_desc.friction = 0.72f;
	body_desc.restitution = 0.12f;
	body_desc.linear_velocity = initial_velocity;
	body_desc.angular_velocity = glm::vec3(0.8f, 0.0f, 0.35f);

	SimulatedModel simulated;
	simulated.body = physics.CreateBody(body_desc);
	simulated.renderables = model.RegisterRenderables(
		MakeTransform(floor_position, yaw_degrees, kVisualScale),
		RenderableFlagDynamic | RenderableFlagCastsShadow | RenderableFlagReceivesShadow);
	simulated.body_to_render = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -kBodyHalfHeight, 0.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(kVisualScale));
	return simulated;
}
#endif
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

#if !defined(CENGINE_ENABLE_OPENGL)
	fprintf(stderr, "OpenGL was requested, but this build was compiled without OpenGL support.\n");
	glfwTerminate();
	return nullptr;
#else
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
#endif
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

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		PhysicsSystem physics(std::make_unique<JoltPhysicsBackend>());
		PhysicsSystemDesc physics_desc;
		if (!physics.Initialize(physics_desc))
		{
			std::cerr << "Failed to initialize physics.\n";
			RenderSystem::Shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
#endif

		// Start with camera.
		Camera view_cam;

		// Init lighting.
		DirectionalLight sun_light =
			DirectionalLight(glm::vec3(-0.75f, -0.35f, -0.35f), glm::vec3(1.0f, 0.96f, 0.9f), 0.9f);
		sun_light.SetCastsShadows(true);
		sun_light.SetShadowResolution(2048);
		sun_light.SetShadowBias(0.0015f, 0.025f);

		AmbientLighting ambient;
		ambient.sky_color = glm::vec3(0.42f, 0.50f, 0.62f);
		ambient.ground_color = glm::vec3(0.18f, 0.14f, 0.10f);
		ambient.intensity = 0.08f;
		ambient.enabled = true;
		RenderSystem::SetAmbientLighting(ambient);

		// Init materials after the render backend.
		Material barrelMaterial(MaterialShaderType::PBRStandard,
			"assets/demo/barrel/results/barrel_albedo.DDS",
			"assets/demo/barrel/results/barrel_normal.DDS",
			"assets/demo/barrel/results/barrel_metallic_roughness_ao.DDS");
		barrelMaterial.material_name = "Deferred barrel";

		Material transparentBarrelMaterial(MaterialShaderType::PBRStandard,
			"assets/demo/barrel/results/barrel_albedo.DDS",
			"assets/demo/barrel/results/barrel_normal.DDS",
			"assets/demo/barrel/results/barrel_metallic_roughness_ao.DDS");
		transparentBarrelMaterial.material_name = "Transparent blend barrel";
		transparentBarrelMaterial.SetRenderMode(MaterialRenderMode::TransparentBlend);
		transparentBarrelMaterial.SetBaseColorFactor(glm::vec4(0.55f, 0.95f, 1.0f, 0.42f));
		transparentBarrelMaterial.SetCastsShadows(false);

		Material floorMaterial(MaterialShaderType::PBRStandard,
			"assets/demo/barrel/results/barrel_albedo.DDS",
			"assets/demo/barrel/results/barrel_normal.DDS",
			"assets/demo/barrel/results/barrel_metallic_roughness_ao.DDS");
		floorMaterial.material_name = "Shadow receiver floor";
		floorMaterial.SetBaseColorFactor(glm::vec4(0.48f, 0.50f, 0.48f, 1.0f));
		floorMaterial.SetCastsShadows(false);

		Mesh floorMesh;
		floorMesh.mesh_name = "Shadow receiver floor";
		floorMesh.AddMaterialMeshData(&floorMaterial, MakeFloorMeshData(8.0f));
		Renderable floorRenderable;
		floorRenderable.mesh = &floorMesh;
		floorRenderable.material = &floorMaterial;
		floorRenderable.local_bounds = floorMesh.GetLocalBounds();
		floorRenderable.world_bounds = TransformBounds(floorRenderable.local_bounds, floorRenderable.transform);
		floorRenderable.flags = RenderableFlagStatic | RenderableFlagReceivesShadow;
		RenderSystem::RegisterRenderable(floorRenderable);

		// Init models after materials.
		Model barrel = ModelImporter::ImportOBJ("assets/demo/barrel/barrel_low.obj",
			{ {"barrel", &barrelMaterial } }, false);
		Model transparentBarrel = ModelImporter::ImportOBJ("assets/demo/barrel/barrel_low.obj",
			{ {"barrel", &transparentBarrelMaterial } }, false);

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		PhysicsBodyDesc floorBodyDesc;
		floorBodyDesc.motion_type = PhysicsMotionType::Static;
		floorBodyDesc.shape_type = PhysicsShapeType::Box;
		floorBodyDesc.position = glm::vec3(0.0f, -0.12f, 0.0f);
		floorBodyDesc.box_half_extents = glm::vec3(8.0f, 0.12f, 8.0f);
		floorBodyDesc.friction = 0.8f;
		physics.CreateBody(floorBodyDesc);

		std::vector<SimulatedModel> simulatedModels;
		simulatedModels.push_back(CreateDynamicBarrel(physics, barrel,
			glm::vec3(-1.8f, 3.0f, 0.0f), -18.0f, glm::vec3(1.2f, 0.0f, -0.25f)));
		simulatedModels.push_back(CreateDynamicBarrel(physics, barrel,
			glm::vec3(1.8f, 4.7f, -0.25f), 22.0f, glm::vec3(-1.0f, 0.0f, 0.2f)));
		simulatedModels.push_back(CreateDynamicBarrel(physics, barrel,
			glm::vec3(0.0f, 6.4f, -2.15f), 0.0f, glm::vec3(0.25f, 0.0f, 0.8f)));
#else
		barrel.RegisterRenderables(MakeTransform(glm::vec3(-1.8f, 0.0f, 0.0f), -18.0f, 1.0f));
		barrel.RegisterRenderables(MakeTransform(glm::vec3(1.8f, 0.0f, -0.25f), 22.0f, 1.0f));
		barrel.RegisterRenderables(MakeTransform(glm::vec3(0.0f, 0.0f, -2.15f), 0.0f, 1.0f));
#endif

		const uint32_t transparent_flags = RenderableFlagStatic | RenderableFlagReceivesShadow;
		transparentBarrel.RegisterRenderables(MakeTransform(glm::vec3(-0.95f, 0.0f, 0.85f), 12.0f, 0.92f),
			transparent_flags);
		transparentBarrel.RegisterRenderables(MakeTransform(glm::vec3(1.05f, 0.0f, 0.95f), -28.0f, 0.92f),
			transparent_flags);

		double lastTime = glfwGetTime();
		do {
			double current = glfwGetTime();
			float deltaTime = float(current - lastTime);
			lastTime = current;

			control.Update(&view_cam, deltaTime);

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
			physics.Step(deltaTime);
			for (const SimulatedModel& simulated : simulatedModels)
			{
				const glm::mat4 bodyTransform = physics.GetBodyTransform(simulated.body);
				const glm::mat4 renderTransform = bodyTransform * simulated.body_to_render;
				for (RenderableHandle renderable : simulated.renderables)
				{
					RenderSystem::UpdateRenderableTransform(renderable, renderTransform);
				}
			}
#endif

			const float sun_angle = static_cast<float>(current) * 0.35f;
			const glm::vec3 sun_direction =
				glm::normalize(glm::vec3(std::sin(sun_angle), -0.35f, std::cos(sun_angle)));
			sun_light.SetDirection(sun_direction);

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
