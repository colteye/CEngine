//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/main.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/store.h"
#include "assets/scene_asset.h"
#ifdef CENGINE_ENABLE_AUDIO
#include "audio/audio_system.h"
#endif
#include "context.h"
#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "imgui_platform.h"
#include "input/actions.h"
#include "input/sdl/sdl_input_backend.h"
#include "input/input_system.h"
#include "physics/physics_system.h"
#include "renderer/camera.h"
#include "renderer/render_system.h"
#include "scene/scene.h"
#include "window/window_system.h"

#ifdef CENGINE_ENABLE_OPENGL
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace Renderer = CEngine::Renderer;

namespace
{

/**
 * @brief TODO: Describe UseExecutableDirectory.
 *
 * @param executable TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool UseExecutableDirectory(const char *executable)
{
    if (executable == nullptr)
    {
        return false;
    }
    std::error_code error;
    const std::filesystem::path path = std::filesystem::absolute(executable, error);
    if (error || path.parent_path().empty())
    {
        return false;
    }
    std::filesystem::current_path(path.parent_path(), error);
    return !error;
}

/**
 * @brief TODO: Describe ParseScenePath.
 *
 * @param argc TODO: Describe this parameter.
 * @param argv TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::filesystem::path ParseScenePath(int argc, char **argv)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::filesystem::path path(argv[index]);
        if (path.extension() == ".cscene")
        {
            return std::filesystem::absolute(path);
        }
    }
    return {};
}

/**
 * @brief TODO: Describe FindSponzaScene.
 *
 * @param executable TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::filesystem::path FindSponzaScene(const char *executable)
{
    constexpr std::string_view RelativeScene = "assets/compiled/sponza/Sponza.cscene";
    std::error_code error;
    std::filesystem::path search_root = std::filesystem::current_path(error);
    if (!error)
    {
        for (std::filesystem::path path = search_root; !path.empty(); path = path.parent_path())
        {
            const std::filesystem::path candidate = path / RelativeScene;
            if (std::filesystem::is_regular_file(candidate))
            {
                return std::filesystem::absolute(candidate);
            }
            if (path == path.root_path())
            {
                break;
            }
        }
    }

    if (executable == nullptr)
    {
        return {};
    }
    search_root = std::filesystem::absolute(executable, error).parent_path();
    if (error)
    {
        return {};
    }
    for (std::filesystem::path path = search_root; !path.empty(); path = path.parent_path())
    {
        const std::filesystem::path candidate = path / RelativeScene;
        if (std::filesystem::is_regular_file(candidate))
        {
            return std::filesystem::absolute(candidate);
        }
        if (path == path.root_path())
        {
            break;
        }
    }
    return {};
}

/**
 * @brief TODO: Describe ProjectRootForScene.
 *
 * @param scene_path TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::filesystem::path ProjectRootForScene(const std::filesystem::path &scene_path)
{
    for (std::filesystem::path path = scene_path.parent_path(); !path.empty(); path = path.parent_path())
    {
        if (path.filename() == "assets")
        {
            return path.parent_path();
        }
        if (path == path.root_path())
        {
            break;
        }
    }
    return std::filesystem::current_path();
}

#ifdef CENGINE_ENABLE_OPENGL
/**
 * @brief TODO: Describe ActivePlayer.
 *
 * @param scene TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Viewer::PlayerEntity *ActivePlayer(CEngine::Scene::Scene &scene)
{
    CEngine::Scene::Entity *entity = scene.GetEntity(scene.Settings().active_entity);
    if (entity != nullptr && entity->Classname() == "player")
    {
        return dynamic_cast<Viewer::PlayerEntity *>(entity);
    }
    for (const auto &candidate : scene.Entities())
    {
        if (candidate != nullptr && candidate->Classname() == "player")
        {
            return dynamic_cast<Viewer::PlayerEntity *>(candidate.get());
        }
    }
    return nullptr;
}

/**
 * @brief TODO: Describe DrawTuningPanel.
 *
 * @param renderer TODO: Describe this parameter.
 * @param player TODO: Describe this parameter.
 */
void DrawTuningPanel(Renderer::RenderSystem &renderer, Viewer::PlayerEntity *player)
{
    ImGui::SetNextWindowBgAlpha(0.82f);
    if (!ImGui::Begin("Viewer tuning"))
    {
        ImGui::End();
        return;
    }

    Renderer::Camera camera = renderer.ActiveCamera();
    float vertical_fov = player != nullptr ? player->vertical_fov_radians : camera.vertical_fov_radians;
    float near_clip = player != nullptr ? player->near_clip : camera.near_clip;
    float far_clip = player != nullptr ? player->far_clip : camera.far_clip;
    bool camera_changed = false;
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        camera_changed |= ImGui::SliderAngle("Vertical FOV", &vertical_fov, 20.0f, 110.0f);
        camera_changed |= ImGui::SliderFloat("Near clip", &near_clip, 0.01f, 2.0f);
        camera_changed |= ImGui::SliderFloat("Far clip", &far_clip, 10.0f, 5000.0f);
    }
    if (camera_changed)
    {
        camera.vertical_fov_radians = vertical_fov;
        camera.near_clip = near_clip;
        camera.far_clip = far_clip;
        renderer.UpdateCamera(camera);
        if (player != nullptr)
        {
            player->vertical_fov_radians = vertical_fov;
            player->near_clip = near_clip;
            player->far_clip = far_clip;
        }
    }

    Renderer::ImageBasedLighting sky = renderer.GetImageBasedLighting();
    if (ImGui::CollapsingHeader("Sky / IBL", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &sky.enabled);
        ImGui::SliderFloat("Visible sky", &sky.sky_intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("IBL lighting", &sky.lighting_intensity, 0.0f, 3.0f);
        ImGui::SliderAngle("Sky rotation", &sky.rotation_radians, -180.0f, 180.0f);
    }
    renderer.SetImageBasedLighting(sky);

    Renderer::ExponentialHeightFog fog = renderer.GetExponentialHeightFog();
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
    renderer.SetExponentialHeightFog(fog);

    Renderer::PostProcessSettings post = renderer.GetPostProcessSettings();
    Renderer::SSAOSettings ssao = renderer.GetSSAOSettings();
    if (ImGui::CollapsingHeader("Screen-space ambient occlusion", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled##ssao", &ssao.enabled);
        ImGui::SliderFloat("Radius##ssao", &ssao.radius, 0.05f, 3.0f);
        ImGui::SliderFloat("Bias##ssao", &ssao.bias, 0.0f, 0.15f);
        ImGui::SliderFloat("Intensity##ssao", &ssao.intensity, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast##ssao", &ssao.contrast, 0.25f, 3.0f);
    }
    renderer.SetSSAOSettings(ssao);

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
    renderer.SetPostProcessSettings(post);
    ImGui::TextUnformatted("Tab: UI/camera control. Shift: show/hide panel. Values are runtime-only.");
    ImGui::End();
}

/**
 * @brief TODO: Describe DrawFpsCounter.
 *
 * @param delta_seconds TODO: Describe this parameter.
 */
void DrawFpsCounter(float delta_seconds)
{
    static float smoothed_fps = 60.0f;
    const float instant_fps = delta_seconds > 0.00001f ? 1.0f / delta_seconds : smoothed_fps;
    smoothed_fps += (instant_fps - smoothed_fps) * 0.08f;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, viewport->WorkPos.y + 12.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.45f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    ImGui::Begin("Frame rate", nullptr, flags);
    ImGui::Text("%.0f FPS", smoothed_fps);
    ImGui::End();
}

/**
 * @brief TODO: Describe BeginTuningFrame.
 */
void BeginTuningFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    Viewer::ImGuiPlatform::NewFrame();
    ImGui::NewFrame();
}

/**
 * @brief TODO: Describe EndTuningFrame.
 */
void EndTuningFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif

/**
 * @brief TODO: Describe RunScene.
 *
 * @param window TODO: Describe this parameter.
 * @param scene_path TODO: Describe this parameter.
 * @param project_root TODO: Describe this parameter.
 * @param renderer TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
int RunScene(CEngine::Window::WindowSystem &window, const std::filesystem::path &scene_path,
             const std::filesystem::path &project_root, Renderer::RenderSystem &renderer)
{
    CEngine::Assets::Store assets(project_root);
    CEngine::Input::InputSystem input(std::make_unique<CEngine::Input::SdlInputBackend>(window));
    const Viewer::Actions actions = Viewer::RegisterActions(input);
#ifdef CENGINE_ENABLE_AUDIO
    CEngine::Audio::AudioSystem audio;
    if (!audio.Initialize())
    {
        std::cerr << "Failed to initialize required audio.\n";
        return 1;
    }
#endif
    CEngine::Entities::EntityFactory entity_factory;
    entity_factory.Register<Viewer::PlayerEntity, Viewer::Generated::Player>("player", actions);
    std::unique_ptr<CEngine::Scene::Scene> scene = CEngine::Assets::LoadScene(scene_path, assets, entity_factory);
    if (scene == nullptr)
    {
        return 1;
    }
    Renderer::AmbientLighting ambient;
    ambient.sky_color = scene->Settings().ambient_color;
    ambient.ground_color = scene->Settings().ambient_color * 0.35f;
    ambient.intensity = scene->Settings().exposure;
    ambient.enabled = glm::any(glm::greaterThan(scene->Settings().ambient_color, glm::vec3(0.0f)));
    renderer.SetAmbientLighting(ambient);

#ifdef CENGINE_ENABLE_JOLT_PHYSICS
    PhysicsSystem physics;
    PhysicsSystemDesc physics_desc;
    physics_desc.gravity = scene->Settings().gravity;
    if (!physics.Initialize(physics_desc))
    {
        std::cerr << "Failed to initialize scene physics.\n";
        return 1;
    }
#endif

    CEngine::Context context;
    context.assets = &assets;
    context.rendering = &renderer;
    context.input = &input;
#ifdef CENGINE_ENABLE_AUDIO
    context.audio = &audio;
#endif
#ifdef CENGINE_ENABLE_JOLT_PHYSICS
    context.physics = &physics;
#endif
    try
    {
        scene->Activate(context);
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Failed to activate scene entities: " << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Failed to activate scene entities.\n";
        return 1;
    }

    const glm::ivec2 initial_size = window.DrawableSize();
    int framebuffer_width = initial_size.x;
    int framebuffer_height = initial_size.y;
    if (framebuffer_width <= 0 || framebuffer_height <= 0 || !renderer.Resize(framebuffer_width, framebuffer_height))
    {
        std::cerr << "Failed to size the renderer for the window.\n";
        return 1;
    }
    renderer.SetCameraAspectRatio(static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height));
#ifdef CENGINE_ENABLE_OPENGL
    Viewer::PlayerEntity *player = ActivePlayer(*scene);
    bool ui_input_mode = true;
    bool show_tuning_panel = true;
    bool tab_was_down = false;
    bool shift_was_down = false;
    input.SetPointerCaptured(false);
#else
    input.SetPointerCaptured(true);
#endif
    double previous_time = window.TimeSeconds();
    double simulation_accumulator = 0.0;
    constexpr double FixedDelta = 1.0 / 60.0;
    constexpr int MaxStepsPerFrame = 4;
    while (!window.ShouldClose())
    {
#ifdef CENGINE_ENABLE_OPENGL
        window.PollEvents(&Viewer::ImGuiPlatform::ProcessEvent, nullptr);
#else
        window.PollEvents();
#endif
        const glm::ivec2 current_size = window.DrawableSize();
        const int current_width = current_size.x;
        const int current_height = current_size.y;
        if (current_width <= 0 || current_height <= 0)
        {
#ifdef CENGINE_ENABLE_OPENGL
            window.WaitEvents(100, &Viewer::ImGuiPlatform::ProcessEvent, nullptr);
#else
            window.WaitEvents(100);
#endif
            previous_time = window.TimeSeconds();
            simulation_accumulator = 0.0;
            continue;
        }
        if (current_width != framebuffer_width || current_height != framebuffer_height)
        {
            if (!renderer.Resize(current_width, current_height))
            {
                std::cerr << "Failed to resize the renderer.\n";
                return 1;
            }
            framebuffer_width = current_width;
            framebuffer_height = current_height;
            renderer.SetCameraAspectRatio(static_cast<float>(framebuffer_width) /
                                          static_cast<float>(framebuffer_height));
        }
        const double current_time = window.TimeSeconds();
        const float delta_seconds = static_cast<float>(std::min(current_time - previous_time, 0.25));
        previous_time = current_time;
        input.BeginFrame();
        if (input.IsDown(CEngine::Input::Key::Escape))
        {
            window.RequestClose();
            continue;
        }

#ifdef CENGINE_ENABLE_OPENGL
        const bool tab_down = input.IsDown(CEngine::Input::Key::Tab);
        if (tab_down && !tab_was_down)
        {
            ui_input_mode = !ui_input_mode;
            input.SetPointerCaptured(!ui_input_mode);
        }
        tab_was_down = tab_down;
        const bool shift_down =
            input.IsDown(CEngine::Input::Key::LeftShift) || input.IsDown(CEngine::Input::Key::RightShift);
        if (shift_down && !shift_was_down)
        {
            show_tuning_panel = !show_tuning_panel;
        }
        shift_was_down = shift_down;
        if (ui_input_mode)
        {
            input.Set(actions.move_forward, 0.0f);
            input.Set(actions.move_right, 0.0f);
            input.Set(actions.look_yaw, 0.0f);
            input.Set(actions.look_pitch, 0.0f);
            input.Set(actions.sprint, 0.0f);
            input.Set(actions.jump, 0.0f);
        }
#endif

        simulation_accumulator += delta_seconds;
        int simulation_steps = 0;
        while (simulation_accumulator >= FixedDelta && simulation_steps < MaxStepsPerFrame)
        {
            scene->Update(context, static_cast<float>(FixedDelta));
            simulation_accumulator -= FixedDelta;
            ++simulation_steps;
        }
        if (simulation_steps == MaxStepsPerFrame && simulation_accumulator >= FixedDelta)
        {
            simulation_accumulator = std::fmod(simulation_accumulator, FixedDelta);
        }

#ifdef CENGINE_ENABLE_AUDIO
        const Renderer::Camera &active_camera = renderer.ActiveCamera();
        CEngine::Audio::Listener listener;
        listener.position = active_camera.position;
        listener.forward = active_camera.direction;
        audio.SetListener(listener);
        audio.Update();
#endif

#ifdef CENGINE_ENABLE_OPENGL
        BeginTuningFrame();
        if (show_tuning_panel)
        {
            DrawFpsCounter(delta_seconds);
            DrawTuningPanel(renderer, player);
        }
#endif
        renderer.Render();
#ifdef CENGINE_ENABLE_OPENGL
        EndTuningFrame();
        window.SwapBuffers();
#endif
    }
    return 0;
}

} // namespace

/**
 * @brief TODO: Describe main.
 *
 * @param argc TODO: Describe this parameter.
 * @param argv TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
int main(int argc, char **argv)
{
    std::filesystem::path scene_path = ParseScenePath(argc, argv);
    if (scene_path.empty() && argc == 1)
    {
        scene_path = FindSponzaScene(argc > 0 ? argv[0] : nullptr);
    }
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

    CEngine::Window::WindowSystem window;
    CEngine::Window::WindowDesc window_desc;
#ifdef CENGINE_ENABLE_VULKAN
    window_desc.title = "CEngine - Vulkan";
    window_desc.graphics_api = CEngine::Window::GraphicsApi::Vulkan;
#endif
    if (!window.Initialize(window_desc))
    {
        return 1;
    }

    Renderer::RenderSystem renderer;
    if (!renderer.Initialize(window))
    {
        std::cerr << "Failed to initialize the renderer.\n";
        return 1;
    }

#ifdef CENGINE_ENABLE_OPENGL
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!Viewer::ImGuiPlatform::Initialize(window))
    {
        std::cerr << "Failed to initialize the ImGui SDL3 backend.\n";
        ImGui::DestroyContext();
        renderer.Shutdown();
        return 1;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        std::cerr << "Failed to initialize the ImGui OpenGL backend.\n";
        Viewer::ImGuiPlatform::Shutdown();
        ImGui::DestroyContext();
        renderer.Shutdown();
        return 1;
    }
#endif

    const int result = RunScene(window, scene_path, project_root, renderer);

#ifdef CENGINE_ENABLE_OPENGL
    ImGui_ImplOpenGL3_Shutdown();
    Viewer::ImGuiPlatform::Shutdown();
    ImGui::DestroyContext();
#endif
    renderer.Shutdown();
    window.Shutdown();
    return result;
}
