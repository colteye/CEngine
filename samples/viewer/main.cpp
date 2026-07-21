#include "assets/asset_database.h"
#include "camera.h"
#include "controls.h"
#include "entity/camera_entity.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"
#include "scene/scene_loader.h"
#include "scene/scene_physics_state.h"
#include "scene/scene_render_state.h"

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
#include "physics/jolt/jolt_physics_backend.h"
#endif

#if defined(CENGINE_ENABLE_OPENGL)
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace Renderer = CEngine::Renderer;

namespace {

bool UseExecutableDirectory(const char* executable)
{
	if (executable == nullptr) return false;
	std::error_code error;
	const std::filesystem::path path = std::filesystem::absolute(executable, error);
	if (error || path.parent_path().empty()) return false;
	std::filesystem::current_path(path.parent_path(), error);
	return !error;
}

std::filesystem::path ParseScenePath(int argc, char** argv)
{
	for (int index = 1; index < argc; ++index)
	{
		const std::filesystem::path path(argv[index]);
		if (path.extension() == ".cscene") return std::filesystem::absolute(path);
	}
	return {};
}

std::filesystem::path FindSponzaScene(const char* executable)
{
	constexpr std::string_view relative_scene = "assets/compiled/sponza/Sponza.cscene";
	std::error_code error;
	std::filesystem::path search_root = std::filesystem::current_path(error);
	if (!error)
	{
		for (std::filesystem::path path = search_root; !path.empty(); path = path.parent_path())
		{
			const std::filesystem::path candidate = path / relative_scene;
			if (std::filesystem::is_regular_file(candidate)) return std::filesystem::absolute(candidate);
			if (path == path.root_path()) break;
		}
	}

	if (executable == nullptr) return {};
	search_root = std::filesystem::absolute(executable, error).parent_path();
	if (error) return {};
	for (std::filesystem::path path = search_root; !path.empty(); path = path.parent_path())
	{
		const std::filesystem::path candidate = path / relative_scene;
		if (std::filesystem::is_regular_file(candidate)) return std::filesystem::absolute(candidate);
		if (path == path.root_path()) break;
	}
	return {};
}

std::filesystem::path ProjectRootForScene(const std::filesystem::path& scene_path)
{
	for (std::filesystem::path path = scene_path.parent_path(); !path.empty(); path = path.parent_path())
	{
		if (path.filename() == "assets") return path.parent_path();
		if (path == path.root_path()) break;
	}
	return std::filesystem::current_path();
}

GLFWwindow* CreateWindow()
{
	if (glfwInit() == GLFW_FALSE)
	{
		std::cerr << "Failed to initialize GLFW.\n";
		return nullptr;
	}
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

#if defined(CENGINE_ENABLE_VULKAN)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1024, 768, "CEngine - Vulkan", nullptr, nullptr);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
	GLFWwindow* window = glfwCreateWindow(1024, 768, "CEngine", nullptr, nullptr);
#endif
	if (window == nullptr)
	{
		std::cerr << "Failed to create the CEngine window.\n";
		glfwTerminate();
		return nullptr;
	}

#if defined(CENGINE_ENABLE_OPENGL)
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
	{
		std::cerr << "Failed to initialize OpenGL.\n";
		glfwDestroyWindow(window);
		glfwTerminate();
		return nullptr;
	}
#endif
	return window;
}

void ApplyActiveCamera(const CEngine::Scene::Scene& scene, Camera& camera)
{
	const CEngine::Scene::Entity* entity = scene.GetEntity(scene.Settings().active_camera);
	if (entity == nullptr) return;

	const auto& source = static_cast<const CEngine::Entities::CameraEntity&>(*entity);
	camera.SetPosition(source.GetTransform().position);
	camera.SetAnglesCartesian(glm::normalize(glm::vec3(
		source.GetTransform().world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))));
	camera.SetFOV(glm::degrees(source.vertical_fov_radians));
}

int RunScene(GLFWwindow* window, const std::filesystem::path& scene_path,
	const std::filesystem::path& project_root)
{
	CEngine::Assets::AssetDatabase assets(project_root);
	std::string error;
	std::unique_ptr<CEngine::Scene::Scene> scene =
		CEngine::Scene::LoadScene(scene_path, assets, &error);
	if (scene == nullptr)
	{
		std::cerr << "Failed to load scene: " << error << '\n';
		return 1;
	}
	CEngine::Scene::SceneRenderState render_state;
	if (!render_state.Activate(*scene, assets, &error))
	{
		std::cerr << "Failed to bind scene rendering: " << error << '\n';
		return 1;
	}

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
	PhysicsSystem physics(std::make_unique<JoltPhysicsBackend>());
	PhysicsSystemDesc physics_desc;
	physics_desc.gravity = scene->Settings().gravity;
	if (!physics.Initialize(physics_desc))
	{
		std::cerr << "Failed to initialize scene physics.\n";
		return 1;
	}
	CEngine::Scene::ScenePhysicsState physics_state;
	if (!physics_state.Activate(*scene, physics, &error))
	{
		std::cerr << "Failed to bind scene physics: " << error << '\n';
		return 1;
	}
#endif

	try
	{
		scene->Activate();
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Failed to activate scene entities: " << exception.what() << '\n';
		return 1;
	}
	catch (...)
	{
		std::cerr << "Failed to activate scene entities.\n";
		return 1;
	}

	Camera camera;
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	if (framebuffer_width <= 0 || framebuffer_height <= 0 ||
		!Renderer::RenderSystem::Resize(framebuffer_width, framebuffer_height))
	{
		std::cerr << "Failed to size the renderer for the window.\n";
		return 1;
	}
	camera.SetAspectRatio(static_cast<float>(framebuffer_width) /
		static_cast<float>(framebuffer_height));
	ApplyActiveCamera(*scene, camera);
	Controls controls(window);
	double previous_time = glfwGetTime();
	while (glfwWindowShouldClose(window) == GLFW_FALSE &&
		glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
	{
		int current_width = 0;
		int current_height = 0;
		glfwGetFramebufferSize(window, &current_width, &current_height);
		if (current_width <= 0 || current_height <= 0)
		{
			glfwWaitEvents();
			previous_time = glfwGetTime();
			continue;
		}
		if (current_width != framebuffer_width || current_height != framebuffer_height)
		{
			if (!Renderer::RenderSystem::Resize(current_width, current_height))
			{
				std::cerr << "Failed to resize the renderer.\n";
				return 1;
			}
			framebuffer_width = current_width;
			framebuffer_height = current_height;
			camera.SetAspectRatio(static_cast<float>(framebuffer_width) /
				static_cast<float>(framebuffer_height));
		}
		const double current_time = glfwGetTime();
		const float delta_seconds = static_cast<float>(current_time - previous_time);
		previous_time = current_time;
		controls.Update(&camera, delta_seconds);

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		physics_state.Step(delta_seconds);
		render_state.UpdateDynamic(*scene);
#endif
		Renderer::RenderSystem::Render();
#if defined(CENGINE_ENABLE_OPENGL)
		glfwSwapBuffers(window);
#endif
		glfwPollEvents();
	}
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	std::filesystem::path scene_path = ParseScenePath(argc, argv);
	if (scene_path.empty() && argc == 1) scene_path = FindSponzaScene(argc > 0 ? argv[0] : nullptr);
	if (scene_path.empty())
	{
		std::cerr << "Sponza sample not found. Usage: CEngine [path-to-scene.cscene]\n";
		return 2;
	}
	const std::filesystem::path project_root = ProjectRootForScene(scene_path);
	if (!UseExecutableDirectory(argc > 0 ? argv[0] : nullptr))
	{
		std::cerr << "Failed to select the runtime shader directory.\n";
		return 1;
	}

	GLFWwindow* window = CreateWindow();
	if (window == nullptr) return 1;
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (!Renderer::RenderSystem::Initialize(window, width, height))
	{
		std::cerr << "Failed to initialize the renderer.\n";
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	const int result = RunScene(window, scene_path, project_root);
	Renderer::RenderSystem::Shutdown();
	glfwDestroyWindow(window);
	glfwTerminate();
	return result;
}
