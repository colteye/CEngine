#include "assets/asset_database.h"
#include "camera.h"
#include "controls.h"
#include "entity/player_entity.h"
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
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
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

#if defined(CENGINE_ENABLE_OPENGL)
void DrawTuningPanel(Camera& camera)
{
	ImGui::SetNextWindowBgAlpha(0.82f);
	if (!ImGui::Begin("Viewer tuning"))
	{
		ImGui::End();
		return;
	}

	Renderer::ImageBasedLighting sky = Renderer::RenderSystem::GetImageBasedLighting();
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float fov = camera.GetFOV();
		float near_clip = camera.GetNearClip();
		float far_clip = camera.GetFarClip();
		if (ImGui::SliderFloat("Vertical FOV", &fov, 20.0f, 110.0f)) camera.SetFOV(fov);
		if (ImGui::SliderFloat("Near clip", &near_clip, 0.01f, 2.0f)) camera.SetNearClip(near_clip);
		if (ImGui::SliderFloat("Far clip", &far_clip, 10.0f, 500.0f)) camera.SetFarClip(far_clip);
	}

	if (ImGui::CollapsingHeader("Sky / IBL", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enabled", &sky.enabled);
		ImGui::SliderFloat("Visible sky", &sky.sky_intensity, 0.0f, 3.0f);
		ImGui::SliderFloat("IBL lighting", &sky.lighting_intensity, 0.0f, 3.0f);
		ImGui::SliderAngle("Sky rotation", &sky.rotation_radians, -180.0f, 180.0f);
	}
	Renderer::RenderSystem::SetImageBasedLighting(sky);

	Renderer::ExponentialHeightFog fog = Renderer::RenderSystem::GetExponentialHeightFog();
	if (ImGui::CollapsingHeader("Exponential height fog", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enabled##fog", &fog.enabled);
		ImGui::ColorEdit3("Inscatter color", &fog.inscattering_color.x);
		ImGui::DragFloat("Density", &fog.density, 0.0001f, 0.0f, 0.25f, "%.4f");
		ImGui::DragFloat("Height falloff", &fog.height_falloff, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("Base height", &fog.base_height, 0.05f, -50.0f, 50.0f);
		ImGui::DragFloat("Start distance", &fog.start_distance, 0.1f, 0.0f, 100.0f);
		ImGui::SliderFloat("Maximum opacity", &fog.max_opacity, 0.0f, 1.0f);
		ImGui::DragFloat("Cutoff distance", &fog.cutoff_distance, 0.1f, 0.0f, 500.0f);
	}
	Renderer::RenderSystem::SetExponentialHeightFog(fog);

	Renderer::PostProcessSettings post = Renderer::RenderSystem::GetPostProcessSettings();
	Renderer::SSAOSettings ssao = Renderer::RenderSystem::GetSSAOSettings();
	if (ImGui::CollapsingHeader("Screen-space ambient occlusion", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enabled##ssao", &ssao.enabled);
		ImGui::SliderFloat("Radius##ssao", &ssao.radius, 0.05f, 3.0f);
		ImGui::SliderFloat("Bias##ssao", &ssao.bias, 0.0f, 0.15f);
		ImGui::SliderFloat("Intensity##ssao", &ssao.intensity, 0.0f, 2.0f);
		ImGui::SliderFloat("Contrast##ssao", &ssao.contrast, 0.25f, 3.0f);
	}
	Renderer::RenderSystem::SetSSAOSettings(ssao);

	if (ImGui::CollapsingHeader("Screen-space effects", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Bloom", &post.bloom_enabled);
		ImGui::SliderFloat("Bloom threshold", &post.bloom_threshold, 0.0f, 4.0f);
		ImGui::SliderFloat("Bloom intensity", &post.bloom_intensity, 0.0f, 2.0f);
		ImGui::Separator();
		ImGui::Checkbox("Tone mapping", &post.tone_mapping_enabled);
		ImGui::SliderFloat("Exposure", &post.exposure, 0.0f, 3.0f);
		ImGui::SliderFloat("Contrast", &post.contrast, 0.5f, 1.5f);
		ImGui::SliderFloat("Saturation", &post.saturation, 0.0f, 2.0f);
		ImGui::Separator();
		ImGui::Checkbox("Depth of field", &post.depth_of_field_enabled);
		ImGui::SliderFloat("Focus distance", &post.focus_distance, 0.1f, 100.0f);
		ImGui::SliderFloat("Focus range", &post.focus_range, 0.1f, 50.0f);
		ImGui::SliderFloat("DoF strength", &post.depth_of_field_strength, 0.0f, 2.0f);
		ImGui::Separator();
		ImGui::Checkbox("Sun lens flare", &post.sun_lens_flare_enabled);
		ImGui::SliderFloat("Flare intensity", &post.sun_lens_flare_intensity, 0.0f, 2.0f);
		ImGui::SliderFloat("Sun disc size", &post.sun_disc_size, 0.002f, 0.04f);
		ImGui::SliderFloat("Sun edge softness", &post.sun_disc_softness, 0.001f, 0.03f);
	}
	Renderer::RenderSystem::SetPostProcessSettings(post);
	ImGui::TextUnformatted("Tab: UI/camera control. Shift: show/hide panel. Values are runtime-only.");
	ImGui::End();
}

void DrawFpsCounter(float delta_seconds)
{
	static float smoothed_fps = 60.0f;
	const float instant_fps = delta_seconds > 0.00001f ? 1.0f / delta_seconds : smoothed_fps;
	smoothed_fps += (instant_fps - smoothed_fps) * 0.08f;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f,
		viewport->WorkPos.y + 12.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.45f);
	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
	ImGui::Begin("Frame rate", nullptr, flags);
	ImGui::Text("%.0f FPS", smoothed_fps);
	ImGui::End();
}

void BeginTuningFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void EndTuningFrame()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif

CEngine::Entities::PlayerEntity* ActivePlayer(CEngine::Scene::Scene& scene)
{
	CEngine::Scene::Entity* entity = scene.GetEntity(scene.Settings().active_player);
	if (entity == nullptr || entity->Classname() != "player") return nullptr;
	return static_cast<CEngine::Entities::PlayerEntity*>(entity);
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
	// Viewer look: restrained photographic treatment rather than a stylized glow.
	Renderer::PostProcessSettings post_process;
	post_process.bloom_enabled = true;
	post_process.bloom_threshold = 0.85f;
	post_process.bloom_intensity = 10.0f;
	post_process.tone_mapping_enabled = true;
	post_process.exposure = 1.0f;
	post_process.contrast = 1.02f;
	post_process.saturation = 1.0f;
	post_process.depth_of_field_enabled = true;
	post_process.focus_distance = 14.0f;
	post_process.focus_range = 6.0f;
	post_process.depth_of_field_strength = 10.0f;
	post_process.sun_lens_flare_enabled = true;
	post_process.sun_lens_flare_intensity = 2.50f;
	Renderer::RenderSystem::SetPostProcessSettings(post_process);
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
	CEngine::Entities::PlayerEntity* player = ActivePlayer(*scene);
	if (player == nullptr)
		for (const auto& entity : scene->Entities())
			if (entity != nullptr && entity->Classname() == "player") { player = static_cast<CEngine::Entities::PlayerEntity*>(entity.get()); break; }
	if (player != nullptr) camera.ApplyPlayer(*player);
	Controls controls(window);
	bool ui_input_mode = true;
	bool show_tuning_panel = true;
	bool tab_was_down = false;
	bool shift_was_down = false;
	controls.SetCameraInputEnabled(false);
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
		const bool tab_down = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
		if (tab_down && !tab_was_down)
		{
			ui_input_mode = !ui_input_mode;
			controls.SetCameraInputEnabled(!ui_input_mode);
		}
		tab_was_down = tab_down;
		const bool shift_down = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
		if (shift_down && !shift_was_down) show_tuning_panel = !show_tuning_panel;
		shift_was_down = shift_down;

#if defined(CENGINE_ENABLE_OPENGL)
		BeginTuningFrame();
		if (!ui_input_mode && player != nullptr) { controls.Update(&camera, delta_seconds); camera.StoreInPlayer(*player); }
#else
		if (player != nullptr) { controls.Update(&camera, delta_seconds); camera.StoreInPlayer(*player); }
#endif

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
		physics_state.Step(delta_seconds);
		render_state.UpdateDynamic(*scene);
#endif
		Renderer::RenderSystem::Render();
#if defined(CENGINE_ENABLE_OPENGL)
		if (show_tuning_panel)
		{
			DrawFpsCounter(delta_seconds);
			DrawTuningPanel(camera);
		}
		EndTuningFrame();
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

#if defined(CENGINE_ENABLE_OPENGL)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	// Let the backend install chained GLFW callbacks so clicks, scroll, and
	// text entry reach ImGui. The viewer itself polls movement keys directly.
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
#endif

	const int result = RunScene(window, scene_path, project_root);

#if defined(CENGINE_ENABLE_OPENGL)
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
#endif
	Renderer::RenderSystem::Shutdown();
	glfwDestroyWindow(window);
	glfwTerminate();
	return result;
}
