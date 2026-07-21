#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(CENGINE_ENABLE_OPENGL)
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "assets/casset_loader.h"
#include "assets/asset_database.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "controls.h"
#include "camera.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene_loader.h"
#include "scene/scene_render_state.h"
#include "scene/scene_physics_state.h"
#include "scene/scene.h"
#include "entity/camera_entity.h"

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
#include "physics/jolt/jolt_physics_backend.h"
#endif

#include "glm/gtx/string_cast.hpp"
#include "main.h"

namespace Renderer = CEngine::Renderer;

namespace {
bool SetRuntimeWorkingDirectory(const char* argv0)
{
	std::filesystem::path executable_path;

#if defined(__APPLE__)
	uint32_t buffer_size = 0;
	_NSGetExecutablePath(nullptr, &buffer_size);
	std::vector<char> buffer(buffer_size);
	if (_NSGetExecutablePath(buffer.data(), &buffer_size) == 0)
	{
		executable_path = std::filesystem::weakly_canonical(buffer.data());
	}
#endif

	if (executable_path.empty() && argv0 != nullptr)
	{
		executable_path = std::filesystem::absolute(argv0);
	}
	if (executable_path.empty() || executable_path.parent_path().empty())
	{
		return false;
	}

	std::error_code error;
	std::filesystem::current_path(executable_path.parent_path(), error);
	return !error;
}

struct SimulatedModel
{
	PhysicsBodyHandle body = kInvalidPhysicsBodyHandle;
	std::vector<Renderer::RenderableHandle> renderables;
	glm::mat4 body_to_render = glm::mat4(1.0f);
};

struct DemoModel
{
	const Renderer::Mesh* mesh = nullptr;
	Renderer::Material* material = nullptr;

	std::vector<Renderer::RenderableHandle> RegisterRenderables(const glm::mat4& transform,
		uint32_t flags = Renderer::RenderableFlagStatic | Renderer::RenderableFlagCastsShadow | Renderer::RenderableFlagReceivesShadow) const
	{
		std::vector<Renderer::RenderableHandle> handles;
		if (mesh == nullptr || material == nullptr)
		{
			return handles;
		}

		for (const auto& material_mesh_data : mesh->GetMaterialMeshData())
		{
			Renderer::Renderable renderable;
			renderable.mesh = mesh;
			renderable.material = material_mesh_data.first != nullptr ? material_mesh_data.first : material;
			renderable.transform = transform;
			renderable.flags = flags;
			renderable.local_bounds = material_mesh_data.second.local_bounds.valid ?
				material_mesh_data.second.local_bounds : Renderer::CalculateBounds(material_mesh_data.second.vertices);
			renderable.world_bounds = Renderer::TransformBounds(renderable.local_bounds, transform);
			handles.push_back(Renderer::RenderSystem::RegisterRenderable(renderable));
		}

		return handles;
	}
};

struct DemoAssetPart
{
	Renderer::Material material;
	Renderer::Mesh mesh;
	glm::mat4 local_to_asset = glm::mat4(1.0f);
};

struct DemoAsset
{
	std::vector<DemoAssetPart> parts;
	Renderer::Bounds local_bounds;

	std::vector<Renderer::RenderableHandle> RegisterRenderables(const glm::mat4& transform,
		uint32_t flags = Renderer::RenderableFlagStatic | Renderer::RenderableFlagCastsShadow | Renderer::RenderableFlagReceivesShadow) const
	{
		std::vector<Renderer::RenderableHandle> handles;
		for (const DemoAssetPart& part : parts)
		{
			const glm::mat4 part_transform = transform * part.local_to_asset;
			for (const auto& material_mesh_data : part.mesh.GetMaterialMeshData())
			{
				Renderer::Renderable renderable;
				renderable.mesh = &part.mesh;
				renderable.material = material_mesh_data.first != nullptr ? material_mesh_data.first :
					const_cast<Renderer::Material*>(&part.material);
				renderable.transform = part_transform;
				renderable.flags = flags;
				renderable.local_bounds = material_mesh_data.second.local_bounds.valid ?
					material_mesh_data.second.local_bounds : Renderer::CalculateBounds(material_mesh_data.second.vertices);
				renderable.world_bounds = Renderer::TransformBounds(renderable.local_bounds, part_transform);
				handles.push_back(Renderer::RenderSystem::RegisterRenderable(renderable));
			}
		}
		return handles;
	}
};

glm::mat4 MakeTransform(const glm::vec3& position, float yaw_degrees, float scale)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
	transform = glm::rotate(transform, glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
	return glm::scale(transform, glm::vec3(scale));
}

glm::mat4 MatrixFromCAssetRows(const std::array<float, 16>& rows)
{
	glm::mat4 matrix(1.0f);
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			matrix[column][row] = rows[static_cast<std::size_t>(row * 4 + column)];
		}
	}
	return matrix;
}

glm::mat4 MakeFitTransform(const Renderer::Bounds& bounds, const glm::vec3& floor_position,
	float yaw_degrees, float target_height)
{
	if (!bounds.valid)
	{
		return MakeTransform(floor_position, yaw_degrees, 1.0f);
	}

	const glm::vec3 size = bounds.max - bounds.min;
	const float height = std::max(size.y, 0.0001f);
	const float scale = target_height / height;
	const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
	const glm::vec3 foot_center(center.x, bounds.min.y, center.z);

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), floor_position);
	transform = glm::rotate(transform, glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
	transform = glm::scale(transform, glm::vec3(scale));
	return transform * glm::translate(glm::mat4(1.0f), -foot_center);
}

glm::quat MakeYawRotation(float yaw_degrees)
{
	return glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
}

Renderer::MeshData MakeFloorMeshData(float half_extent)
{
	Renderer::MeshData data;
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

std::string ResolveBundlePath(const std::filesystem::path& bundle_path, std::string_view component_path)
{
	const std::filesystem::path path(component_path);
	if (path.is_absolute())
	{
		return path.generic_string();
	}
	return (bundle_path.parent_path() / path).lexically_normal().generic_string();
}

bool FindDemoAssetPaths(const std::filesystem::path& casset_path, std::string& mesh_path, std::string& material_path)
{
	std::string error;
	CEngine::Assets::CAsset asset;
	if (!asset.Load(casset_path, &error))
	{
		std::cerr << "Failed to load root asset " << casset_path << ": " << error << '\n';
		return false;
	}

	for (std::uint32_t object_index = 0; object_index < asset.ObjectCount(); ++object_index)
	{
		CEngine::Assets::CAssetObject object;
		if (!asset.Object(object_index, object))
		{
			continue;
		}

		for (std::uint32_t component_index = 0; component_index < object.component_count; ++component_index)
		{
			CEngine::Assets::CAssetComponent component;
			if (!asset.Component(object, component_index, component))
			{
				continue;
			}

			if (component.kind == CEngine::Assets::CAssetComponentKind::Mesh && mesh_path.empty())
			{
				mesh_path = ResolveBundlePath(casset_path, component.path);
			}
			else if (component.kind == CEngine::Assets::CAssetComponentKind::Material && material_path.empty())
			{
				material_path = ResolveBundlePath(casset_path, component.path);
			}
		}

		if (!mesh_path.empty() && !material_path.empty())
		{
			return true;
		}
	}

	std::cerr << "Root asset " << casset_path << " does not contain both mesh and material components.\n";
	return false;
}

bool LoadDemoAsset(const std::filesystem::path& casset_path, Renderer::Material& material, Renderer::Mesh& mesh)
{
	std::string mesh_path;
	std::string material_path;
	if (!FindDemoAssetPaths(casset_path, mesh_path, material_path))
	{
		return false;
	}

	std::string error;
	if (!CEngine::Assets::LoadMaterialAsset(material_path, material, &error))
	{
		std::cerr << "Failed to load target material asset " << material_path << ": " << error << '\n';
		return false;
	}
	if (!CEngine::Assets::LoadMeshAsset(mesh_path, { &material }, mesh, &error))
	{
		std::cerr << "Failed to load target mesh asset " << mesh_path << ": " << error << '\n';
		return false;
	}
	return true;
}

struct DemoAssetComponentPair
{
	std::string mesh_path;
	std::string material_path;
	glm::mat4 local_to_asset = glm::mat4(1.0f);
};

bool LoadDemoAssetBundle(const std::filesystem::path& casset_path, DemoAsset& bundle)
{
	std::string error;
	CEngine::Assets::CAsset asset;
	if (!asset.Load(casset_path, &error))
	{
		std::cerr << "Failed to load root asset " << casset_path << ": " << error << '\n';
		return false;
	}

	std::vector<DemoAssetComponentPair> pairs;
	for (std::uint32_t object_index = 0; object_index < asset.ObjectCount(); ++object_index)
	{
		CEngine::Assets::CAssetObject object;
		if (!asset.Object(object_index, object))
		{
			continue;
		}

		std::vector<std::string> mesh_paths;
		std::vector<std::string> material_paths;
		for (std::uint32_t component_index = 0; component_index < object.component_count; ++component_index)
		{
			CEngine::Assets::CAssetComponent component;
			if (!asset.Component(object, component_index, component))
			{
				continue;
			}

			if (component.kind == CEngine::Assets::CAssetComponentKind::Mesh)
			{
				mesh_paths.push_back(ResolveBundlePath(casset_path, component.path));
			}
			else if (component.kind == CEngine::Assets::CAssetComponentKind::Material)
			{
				material_paths.push_back(ResolveBundlePath(casset_path, component.path));
			}
		}

		if (mesh_paths.empty() || material_paths.empty())
		{
			continue;
		}

		const glm::mat4 local_to_asset = MatrixFromCAssetRows(object.world_from_local);
		for (std::size_t mesh_index = 0; mesh_index < mesh_paths.size(); ++mesh_index)
		{
			const std::size_t material_index = std::min(mesh_index, material_paths.size() - 1);
			pairs.push_back({ mesh_paths[mesh_index], material_paths[material_index], local_to_asset });
		}
	}

	if (pairs.empty())
	{
		std::cerr << "Root asset " << casset_path << " does not contain loadable mesh/material pairs.\n";
		return false;
	}

	bundle.parts.clear();
	bundle.local_bounds = Renderer::Bounds();
	bundle.parts.reserve(pairs.size());
	for (const DemoAssetComponentPair& pair : pairs)
	{
		DemoAssetPart& part = bundle.parts.emplace_back();
		part.local_to_asset = pair.local_to_asset;
		if (!CEngine::Assets::LoadMaterialAsset(pair.material_path, part.material, &error))
		{
			std::cerr << "Failed to load bundled material asset " << pair.material_path << ": " << error << '\n';
			return false;
		}
		part.material.material_name = std::filesystem::path(pair.material_path).stem().string();
		Renderer::RenderSystem::RegisterMaterial(&part.material);

		if (!CEngine::Assets::LoadMeshAsset(pair.mesh_path, { &part.material }, part.mesh, &error))
		{
			std::cerr << "Failed to load bundled mesh asset " << pair.mesh_path << ": " << error << '\n';
			return false;
		}
		bundle.local_bounds = Renderer::MergeBounds(
			bundle.local_bounds,
			Renderer::TransformBounds(part.mesh.GetLocalBounds(), part.local_to_asset));
	}

	std::cout << "Loaded demo asset bundle " << casset_path << " with " << bundle.parts.size() << " mesh/material part(s).\n";
	return true;
}

std::vector<std::filesystem::path> FindExtraDemoAssetPaths(const std::filesystem::path& assets_root,
	const std::filesystem::path& primary_asset_path)
{
	std::vector<std::filesystem::path> paths;
	if (!std::filesystem::exists(assets_root))
	{
		return paths;
	}

	const std::string primary = primary_asset_path.lexically_normal().generic_string();
	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(assets_root))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".casset")
		{
			continue;
		}
		if (entry.path().lexically_normal().generic_string() == primary)
		{
			continue;
		}
		paths.push_back(entry.path());
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

Renderer::RenderBackendType ParseBackend(int argc, char** argv)
{
	for (int index = 1; index < argc; ++index)
	{
		if (std::strcmp(argv[index], "--vulkan") == 0)
		{
			return Renderer::RenderBackendType::Vulkan;
		}
		if (std::strcmp(argv[index], "--opengl") == 0)
		{
			return Renderer::RenderBackendType::OpenGL;
		}
		if (std::strcmp(argv[index], "--backend") == 0 && index + 1 < argc)
		{
			++index;
			if (std::strcmp(argv[index], "vulkan") == 0)
			{
				return Renderer::RenderBackendType::Vulkan;
			}
			if (std::strcmp(argv[index], "opengl") == 0)
			{
				return Renderer::RenderBackendType::OpenGL;
			}
		}
	}

#if defined(CENGINE_ENABLE_OPENGL)
	return Renderer::RenderBackendType::OpenGL;
#elif defined(CENGINE_ENABLE_VULKAN)
	return Renderer::RenderBackendType::Vulkan;
#else
	return Renderer::RenderBackendType::OpenGL;
#endif
}

std::filesystem::path ParseScenePath(int argc, char** argv)
{
	for (int index = 1; index < argc; ++index)
	{
		if (std::strcmp(argv[index], "--backend") == 0) { ++index; continue; }
		const std::filesystem::path path(argv[index]);
		if (path.extension() == ".cscene") return std::filesystem::absolute(path);
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

int RunSceneViewer(GLFWwindow* window, Renderer::RenderBackendType backend_type,
	const std::filesystem::path& scene_path)
{
	CEngine::Assets::AssetDatabase assets(ProjectRootForScene(scene_path));
	std::string error;
	auto scene = CEngine::Scene::LoadScene(scene_path, assets, &error);
	if (scene == nullptr)
	{
		std::cerr << "Failed to load scene: " << error << '\n';
		return -1;
	}
	CEngine::Scene::SceneRenderState render_state;
	if (!render_state.Activate(*scene, assets, &error))
	{
		std::cerr << "Failed to activate scene rendering: " << error << '\n';
		return -1;
	}
#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
	PhysicsSystem physics(std::make_unique<JoltPhysicsBackend>());
	if (!physics.Initialize({}))
	{
		std::cerr << "Failed to initialize scene physics.\n";
		return -1;
	}
	CEngine::Scene::ScenePhysicsState physics_state;
	if (!physics_state.Activate(*scene, physics, &error))
	{
		std::cerr << "Failed to activate scene physics: " << error << '\n';
		return -1;
	}
#endif

	Camera camera;
	for (const auto& entity : scene->Entities())
	{
		if (entity == nullptr || entity->Classname() != "camera") continue;
		const auto& source = static_cast<const CEngine::Entities::CameraEntity&>(*entity);
		camera.SetPosition(source.GetTransform().position);
		camera.SetAnglesCartesian(glm::normalize(glm::vec3(
			source.GetTransform().world_matrix * glm::vec4(0, 0, -1, 0))));
		camera.SetFOV(glm::degrees(source.vertical_fov_radians));
		break;
	}
	Controls controls(window);
	double previous = glfwGetTime();
	do
	{
		const double current = glfwGetTime();
		const float delta_seconds = static_cast<float>(current - previous);
		controls.Update(&camera, delta_seconds);
		previous = current;
#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		physics_state.Step(delta_seconds);
		render_state.UpdateDynamic(*scene);
#endif
		Renderer::RenderSystem::Render();
		if (backend_type == Renderer::RenderBackendType::OpenGL) glfwSwapBuffers(window);
		glfwPollEvents();
	}
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);
	return 0;
}

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
SimulatedModel CreateDynamicBarrel(PhysicsSystem& physics, const DemoModel& model,
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
		Renderer::RenderableFlagDynamic | Renderer::RenderableFlagCastsShadow | Renderer::RenderableFlagReceivesShadow);
	simulated.body_to_render = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -kBodyHalfHeight, 0.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(kVisualScale));
	return simulated;
}
#endif
} // namespace

GLFWwindow* Initialization(Renderer::RenderBackendType backend_type)
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return nullptr;
	}

	if (backend_type == Renderer::RenderBackendType::Vulkan)
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
	const std::filesystem::path scene_path = ParseScenePath(argc, argv);
	if (!SetRuntimeWorkingDirectory(argc > 0 ? argv[0] : nullptr))
	{
		std::cerr << "Warning: could not use the executable directory for runtime assets.\n";
	}

	const Renderer::RenderBackendType backend_type = ParseBackend(argc, argv);

	// Init control related stuff.
	GLFWwindow* window = Initialization(backend_type);
	if (window == nullptr) return -1;

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	// Time to initialize renderer!
	int window_width, window_height;
	glfwGetFramebufferSize(window, &window_width, &window_height);
	if (!Renderer::RenderSystem::Initialize(backend_type, window, window_width, window_height))
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	if (!scene_path.empty())
	{
		const int result = RunSceneViewer(window, backend_type, scene_path);
		Renderer::RenderSystem::Shutdown();
		glfwDestroyWindow(window);
		glfwTerminate();
		return result;
	}

	{
		Controls control = Controls(window);

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		PhysicsSystem physics(std::make_unique<JoltPhysicsBackend>());
		PhysicsSystemDesc physics_desc;
		if (!physics.Initialize(physics_desc))
		{
			std::cerr << "Failed to initialize physics.\n";
			Renderer::RenderSystem::Shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
#endif

		// Start with camera.
		Camera view_cam;

		// Init lighting.
		Renderer::DirectionalLight sun_light =
			Renderer::DirectionalLight(glm::vec3(-0.75f, -0.35f, -0.35f), glm::vec3(1.0f, 0.96f, 0.9f), 0.5f);
		sun_light.SetCastsShadows(true);
		sun_light.SetShadowResolution(2048);
		sun_light.SetShadowBias(0.0015f, 0.025f);

		Renderer::AmbientLighting ambient;
		ambient.sky_color = glm::vec3(0.42f, 0.50f, 0.62f);
		ambient.ground_color = glm::vec3(0.18f, 0.14f, 0.10f);
		ambient.intensity = 0.08f;
		ambient.enabled = true;
		Renderer::RenderSystem::SetAmbientLighting(ambient);

		// Init demo asset after the render backend. Runtime import is target assets only.
		const std::filesystem::path barrelAssetPath = "assets/barrel/barrel.casset";
		Renderer::Material barrelMaterial;
		Renderer::Mesh barrelMesh;
		if (!LoadDemoAsset(barrelAssetPath, barrelMaterial, barrelMesh))
		{
			Renderer::RenderSystem::Shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
		barrelMaterial.material_name = "Deferred barrel";
		Renderer::RenderSystem::RegisterMaterial(&barrelMaterial);
		DemoModel barrel = { &barrelMesh, &barrelMaterial };

		Renderer::Material transparentBarrelMaterial;
		Renderer::Mesh transparentBarrelMesh;
		if (!LoadDemoAsset(barrelAssetPath, transparentBarrelMaterial, transparentBarrelMesh))
		{
			Renderer::RenderSystem::Shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
		transparentBarrelMaterial.material_name = "Transparent blend barrel";
		transparentBarrelMaterial.SetRenderMode(Renderer::MaterialRenderMode::TransparentBlend);
		transparentBarrelMaterial.SetBaseColorFactor(glm::vec4(0.55f, 0.95f, 1.0f, 0.42f));
		transparentBarrelMaterial.SetCastsShadows(false);
		Renderer::RenderSystem::RegisterMaterial(&transparentBarrelMaterial);

		std::vector<DemoAsset> extraAssets;
		for (const std::filesystem::path& asset_path : FindExtraDemoAssetPaths("assets", barrelAssetPath))
		{
			DemoAsset asset;
			if (LoadDemoAssetBundle(asset_path, asset))
			{
				extraAssets.push_back(std::move(asset));
			}
		}

		Renderer::Material floorMaterial(Renderer::MaterialShaderType::PBRStandard,
			"assets/barrel/textures/barrel_low_barrel_BaseColor.dds",
			"assets/barrel/textures/barrel_low_barrel_Normal.dds",
			"assets/barrel/textures/barrel_low_barrel_Roughness.dds");
		floorMaterial.material_name = "Shadow receiver floor";
		floorMaterial.SetBaseColorFactor(glm::vec4(0.48f, 0.50f, 0.48f, 1.0f));
		floorMaterial.SetCastsShadows(false);
		Renderer::RenderSystem::RegisterMaterial(&floorMaterial);

		Renderer::Mesh floorMesh;
		floorMesh.mesh_name = "Shadow receiver floor";
		floorMesh.AddMaterialMeshData(&floorMaterial, MakeFloorMeshData(8.0f));
		Renderer::Renderable floorRenderable;
		floorRenderable.mesh = &floorMesh;
		floorRenderable.material = &floorMaterial;
		floorRenderable.local_bounds = floorMesh.GetLocalBounds();
		floorRenderable.world_bounds = Renderer::TransformBounds(floorRenderable.local_bounds, floorRenderable.transform);
		floorRenderable.flags = Renderer::RenderableFlagStatic | Renderer::RenderableFlagReceivesShadow;
		Renderer::RenderSystem::RegisterRenderable(floorRenderable);

		DemoModel transparentBarrel = { &transparentBarrelMesh, &transparentBarrelMaterial };

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

		const uint32_t transparent_flags = Renderer::RenderableFlagStatic | Renderer::RenderableFlagReceivesShadow;
		transparentBarrel.RegisterRenderables(MakeTransform(glm::vec3(-0.95f, 0.0f, 0.85f), 12.0f, 0.92f),
			transparent_flags);
		transparentBarrel.RegisterRenderables(MakeTransform(glm::vec3(1.05f, 0.0f, 0.95f), -28.0f, 0.92f),
			transparent_flags);
		for (std::size_t asset_index = 0; asset_index < extraAssets.size(); ++asset_index)
		{
			const float x = (static_cast<float>(asset_index) - static_cast<float>(extraAssets.size() - 1) * 0.5f) * 2.75f;
			extraAssets[asset_index].RegisterRenderables(
				MakeFitTransform(extraAssets[asset_index].local_bounds, glm::vec3(x, 0.0f, 2.2f), 180.0f, 2.2f));
		}

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
				for (Renderer::RenderableHandle renderable : simulated.renderables)
				{
					Renderer::RenderSystem::UpdateRenderableTransform(renderable, renderTransform);
				}
			}
#endif

			const float sun_angle = static_cast<float>(current) * 0.35f;
			const glm::vec3 sun_direction =
				glm::normalize(glm::vec3(std::sin(sun_angle), -0.35f, std::cos(sun_angle)));
			sun_light.SetDirection(sun_direction);

			Renderer::RenderSystem::Render();

			// Swap buffers
			if (backend_type == Renderer::RenderBackendType::OpenGL)
			{
				glfwSwapBuffers(window);
			}
			glfwPollEvents();
		} // Check if the ESC key was pressed or the window was closed
		while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
			glfwWindowShouldClose(window) == 0);

		Renderer::RenderSystem::Shutdown();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
