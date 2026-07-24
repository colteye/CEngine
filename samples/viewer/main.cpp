//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/main.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "animation/animation_system.h"
#include "assets/scene_asset.h"
#include "assets/store.h"
#ifdef CENGINE_ENABLE_AUDIO
#include "audio/audio_system.h"
#endif
#include "context.h"
#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "game/game_coordinator.h"
#include "input/actions.h"
#include "input/input_system.h"
#include "input/sdl/sdl_input_backend.h"
#include "physics/physics_system.h"
#include "renderer/camera.h"
#include "renderer/render_system.h"
#include "scene/scene.h"
#include "ui/ui_system.h"
#include "window/window_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace Renderer = CEngine::Renderer;

namespace
{

struct PlatformEventTargets
{
    CEngine::Input::InputSystem *input = nullptr;
};

void ProcessPlatformEvent(const void *event, void *user_data)
{
    auto *targets = static_cast<PlatformEventTargets *>(user_data);
    if (targets != nullptr && targets->input != nullptr)
    {
        targets->input->ProcessPlatformEvent(event);
    }
}

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

std::string DisplayFloat(float value)
{
    char buffer[32]{};
    const float magnitude = std::abs(value);
    const char *format = magnitude < 0.01f ? "%.4f" : (magnitude < 10.0f ? "%.2f" : "%.0f");
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

float EventFloat(const CEngine::UI::UiEvent &event, float fallback)
{
    char *end = nullptr;
    const float value = std::strtof(event.value.c_str(), &end);
    return end != event.value.c_str() && std::isfinite(value) ? value : fallback;
}

bool SetTuningValue(CEngine::UI::UISystem &ui, CEngine::UI::UiScreenHandle screen,
                    std::string_view id, float value)
{
    const std::string label_id = std::string(id) + "-value";
    return ui.SetValue(screen, id, value) && ui.SetText(screen, label_id, DisplayFloat(value));
}

struct FpsDisplay
{
    void Update(CEngine::UI::UISystem &ui, CEngine::UI::UiScreenHandle hud, float delta_seconds)
    {
        const float instant = delta_seconds > 0.00001f ? 1.0f / delta_seconds : smoothed;
        smoothed += (instant - smoothed) * 0.08f;
        update_elapsed += delta_seconds;
        if (update_elapsed >= 0.25f)
        {
            const int current = static_cast<int>(std::lround(smoothed));
            if (current != displayed)
            {
                ui.SetText(hud, "fps-value", std::to_string(current));
                displayed = current;
            }
            update_elapsed = 0.0f;
        }
    }

    float smoothed = 60.0f;
    float update_elapsed = 1.0f;
    int displayed = -1;
};

#ifdef CENGINE_ENABLE_OPENGL
constexpr std::array<std::string_view, 37> TuningControls = {
    "camera-fov",       "camera-near",      "camera-far",       "sky-enabled",
    "sky-visible",      "sky-lighting",     "sky-rotation",     "fog-enabled",
    "fog-red",          "fog-green",        "fog-blue",         "fog-density",
    "fog-falloff",      "fog-base",         "fog-start",        "fog-opacity",
    "fog-cutoff",       "ssao-enabled",     "ssao-radius",      "ssao-bias",
    "ssao-intensity",   "ssao-contrast",    "bloom-enabled",    "bloom-threshold",
    "bloom-intensity",  "tone-enabled",     "tone-exposure",    "tone-contrast",
    "tone-saturation",  "dof-enabled",      "dof-distance",     "dof-range",
    "dof-strength",     "flare-enabled",    "flare-intensity",  "flare-size",
    "flare-softness",
};

bool ConfigureTuningUi(CEngine::UI::UISystem &ui, CEngine::UI::UiScreenHandle screen,
                       Renderer::RenderSystem &renderer, Viewer::PlayerEntity *player)
{
    const Renderer::Camera camera = renderer.ActiveCamera();
    const Renderer::ImageBasedLighting sky = renderer.GetImageBasedLighting();
    const Renderer::ExponentialHeightFog fog = renderer.GetExponentialHeightFog();
    const Renderer::SSAOSettings ssao = renderer.GetSSAOSettings();
    const Renderer::PostProcessSettings post = renderer.GetPostProcessSettings();

    bool result = true;
    result &= SetTuningValue(ui, screen, "camera-fov",
                             glm::degrees(player != nullptr ? player->vertical_fov_radians
                                                            : camera.vertical_fov_radians));
    result &= SetTuningValue(ui, screen, "camera-near", player != nullptr ? player->near_clip : camera.near_clip);
    result &= SetTuningValue(ui, screen, "camera-far", player != nullptr ? player->far_clip : camera.far_clip);
    result &= ui.SetChecked(screen, "sky-enabled", sky.enabled);
    result &= SetTuningValue(ui, screen, "sky-visible", sky.sky_intensity);
    result &= SetTuningValue(ui, screen, "sky-lighting", sky.lighting_intensity);
    result &= SetTuningValue(ui, screen, "sky-rotation", glm::degrees(sky.rotation_radians));
    result &= ui.SetChecked(screen, "fog-enabled", fog.enabled);
    result &= SetTuningValue(ui, screen, "fog-red", fog.inscattering_color.r);
    result &= SetTuningValue(ui, screen, "fog-green", fog.inscattering_color.g);
    result &= SetTuningValue(ui, screen, "fog-blue", fog.inscattering_color.b);
    result &= SetTuningValue(ui, screen, "fog-density", fog.density);
    result &= SetTuningValue(ui, screen, "fog-falloff", fog.height_falloff);
    result &= SetTuningValue(ui, screen, "fog-base", fog.base_height);
    result &= SetTuningValue(ui, screen, "fog-start", fog.start_distance);
    result &= SetTuningValue(ui, screen, "fog-opacity", fog.max_opacity);
    result &= SetTuningValue(ui, screen, "fog-cutoff", fog.cutoff_distance);
    result &= ui.SetChecked(screen, "ssao-enabled", ssao.enabled);
    result &= SetTuningValue(ui, screen, "ssao-radius", ssao.radius);
    result &= SetTuningValue(ui, screen, "ssao-bias", ssao.bias);
    result &= SetTuningValue(ui, screen, "ssao-intensity", ssao.intensity);
    result &= SetTuningValue(ui, screen, "ssao-contrast", ssao.contrast);
    result &= ui.SetChecked(screen, "bloom-enabled", post.bloom_enabled);
    result &= SetTuningValue(ui, screen, "bloom-threshold", post.bloom_threshold);
    result &= SetTuningValue(ui, screen, "bloom-intensity", post.bloom_intensity);
    result &= ui.SetChecked(screen, "tone-enabled", post.tone_mapping_enabled);
    result &= SetTuningValue(ui, screen, "tone-exposure", post.exposure);
    result &= SetTuningValue(ui, screen, "tone-contrast", post.contrast);
    result &= SetTuningValue(ui, screen, "tone-saturation", post.saturation);
    result &= ui.SetChecked(screen, "dof-enabled", post.depth_of_field_enabled);
    result &= SetTuningValue(ui, screen, "dof-distance", post.focus_distance);
    result &= SetTuningValue(ui, screen, "dof-range", post.focus_range);
    result &= SetTuningValue(ui, screen, "dof-strength", post.depth_of_field_strength);
    result &= ui.SetChecked(screen, "flare-enabled", post.sun_lens_flare_enabled);
    result &= SetTuningValue(ui, screen, "flare-intensity", post.sun_lens_flare_intensity);
    result &= SetTuningValue(ui, screen, "flare-size", post.sun_disc_size);
    result &= SetTuningValue(ui, screen, "flare-softness", post.sun_disc_softness);

    for (std::string_view id : TuningControls)
    {
        result &= ui.BindChange(screen, id, std::string(id));
    }
    return result;
}

void ApplyTuningEvent(const CEngine::UI::UiEvent &event, CEngine::UI::UISystem &ui,
                      CEngine::UI::UiScreenHandle screen, Renderer::RenderSystem &renderer,
                      Viewer::PlayerEntity *player)
{
    const std::string_view action = event.action;
    if (action.starts_with("camera-"))
    {
        Renderer::Camera camera = renderer.ActiveCamera();
        if (action == "camera-fov")
            camera.vertical_fov_radians = glm::radians(EventFloat(event, glm::degrees(camera.vertical_fov_radians)));
        else if (action == "camera-near")
            camera.near_clip = EventFloat(event, camera.near_clip);
        else if (action == "camera-far")
            camera.far_clip = EventFloat(event, camera.far_clip);
        renderer.UpdateCamera(camera);
        if (player != nullptr)
        {
            player->vertical_fov_radians = camera.vertical_fov_radians;
            player->near_clip = camera.near_clip;
            player->far_clip = camera.far_clip;
        }
    }
    else if (action.starts_with("sky-"))
    {
        Renderer::ImageBasedLighting sky = renderer.GetImageBasedLighting();
        if (action == "sky-enabled")
            sky.enabled = event.checked;
        else if (action == "sky-visible")
            sky.sky_intensity = EventFloat(event, sky.sky_intensity);
        else if (action == "sky-lighting")
            sky.lighting_intensity = EventFloat(event, sky.lighting_intensity);
        else if (action == "sky-rotation")
            sky.rotation_radians = glm::radians(EventFloat(event, glm::degrees(sky.rotation_radians)));
        renderer.SetImageBasedLighting(sky);
    }
    else if (action.starts_with("fog-"))
    {
        Renderer::ExponentialHeightFog fog = renderer.GetExponentialHeightFog();
        if (action == "fog-enabled")
            fog.enabled = event.checked;
        else if (action == "fog-red")
            fog.inscattering_color.r = EventFloat(event, fog.inscattering_color.r);
        else if (action == "fog-green")
            fog.inscattering_color.g = EventFloat(event, fog.inscattering_color.g);
        else if (action == "fog-blue")
            fog.inscattering_color.b = EventFloat(event, fog.inscattering_color.b);
        else if (action == "fog-density")
            fog.density = EventFloat(event, fog.density);
        else if (action == "fog-falloff")
            fog.height_falloff = EventFloat(event, fog.height_falloff);
        else if (action == "fog-base")
            fog.base_height = EventFloat(event, fog.base_height);
        else if (action == "fog-start")
            fog.start_distance = EventFloat(event, fog.start_distance);
        else if (action == "fog-opacity")
            fog.max_opacity = EventFloat(event, fog.max_opacity);
        else if (action == "fog-cutoff")
            fog.cutoff_distance = EventFloat(event, fog.cutoff_distance);
        renderer.SetExponentialHeightFog(fog);
    }
    else if (action.starts_with("ssao-"))
    {
        Renderer::SSAOSettings ssao = renderer.GetSSAOSettings();
        if (action == "ssao-enabled")
            ssao.enabled = event.checked;
        else if (action == "ssao-radius")
            ssao.radius = EventFloat(event, ssao.radius);
        else if (action == "ssao-bias")
            ssao.bias = EventFloat(event, ssao.bias);
        else if (action == "ssao-intensity")
            ssao.intensity = EventFloat(event, ssao.intensity);
        else if (action == "ssao-contrast")
            ssao.contrast = EventFloat(event, ssao.contrast);
        renderer.SetSSAOSettings(ssao);
    }
    else
    {
        Renderer::PostProcessSettings post = renderer.GetPostProcessSettings();
        if (action == "bloom-enabled")
            post.bloom_enabled = event.checked;
        else if (action == "bloom-threshold")
            post.bloom_threshold = EventFloat(event, post.bloom_threshold);
        else if (action == "bloom-intensity")
            post.bloom_intensity = EventFloat(event, post.bloom_intensity);
        else if (action == "tone-enabled")
            post.tone_mapping_enabled = event.checked;
        else if (action == "tone-exposure")
            post.exposure = EventFloat(event, post.exposure);
        else if (action == "tone-contrast")
            post.contrast = EventFloat(event, post.contrast);
        else if (action == "tone-saturation")
            post.saturation = EventFloat(event, post.saturation);
        else if (action == "dof-enabled")
            post.depth_of_field_enabled = event.checked;
        else if (action == "dof-distance")
            post.focus_distance = EventFloat(event, post.focus_distance);
        else if (action == "dof-range")
            post.focus_range = EventFloat(event, post.focus_range);
        else if (action == "dof-strength")
            post.depth_of_field_strength = EventFloat(event, post.depth_of_field_strength);
        else if (action == "flare-enabled")
            post.sun_lens_flare_enabled = event.checked;
        else if (action == "flare-intensity")
            post.sun_lens_flare_intensity = EventFloat(event, post.sun_lens_flare_intensity);
        else if (action == "flare-size")
            post.sun_disc_size = EventFloat(event, post.sun_disc_size);
        else if (action == "flare-softness")
            post.sun_disc_softness = EventFloat(event, post.sun_disc_softness);
        renderer.SetPostProcessSettings(post);
    }

    if (!event.value.empty())
    {
        ui.SetText(screen, std::string(action) + "-value", DisplayFloat(EventFloat(event, 0.0f)));
    }
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
    CEngine::Animations::AnimationSystem animations;
    if (!animations.Initialize())
    {
        std::cerr << "Failed to initialize skeletal animation.\n";
        return 1;
    }
    CEngine::Input::InputSystem input(std::make_unique<CEngine::Input::SdlInputBackend>(window));
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    CEngine::UI::UISystem ui;
    if (!ui.Initialize(window, std::filesystem::current_path()) ||
        !ui.LoadFont("ui/fonts/LatoLatin-Regular.ttf") ||
        !ui.LoadFont("ui/fonts/LatoLatin-Bold.ttf"))
    {
        std::cerr << "Failed to initialize game UI.\n";
        return 1;
    }
    const CEngine::UI::UiScreenHandle start_menu = ui.LoadScreen("ui/viewer/start_menu.rml");
    const CEngine::UI::UiScreenHandle hud = ui.LoadScreen("ui/viewer/hud.rml");
#ifdef CENGINE_ENABLE_OPENGL
    const CEngine::UI::UiScreenHandle tuning_menu = ui.LoadScreen("ui/viewer/tuning.rml");
#endif
    if (!start_menu || !hud ||
#ifdef CENGINE_ENABLE_OPENGL
        !tuning_menu ||
#endif
        !ui.BindClick(start_menu, "start-button", "start_game") || !ui.Show(start_menu, true) ||
        !ui.Show(hud))
    {
        std::cerr << "Failed to load the viewer UI.\n";
        return 1;
    }
#ifdef CENGINE_ENABLE_AUDIO
    CEngine::Audio::AudioSystem audio;
    if (!audio.Initialize())
    {
        std::cerr << "Failed to initialize required audio.\n";
        return 1;
    }
#endif
    CEngine::Entities::EntityFactory entity_factory;
    Viewer::GameCoordinator game(actions);
    if (!game.RegisterEntityTypes(entity_factory))
    {
        std::cerr << "Failed to register viewer game entities.\n";
        return 1;
    }
    std::unique_ptr<CEngine::Scene::Scene> scene = CEngine::Assets::LoadScene(scene_path, assets, entity_factory);
    if (scene == nullptr)
    {
        return 1;
    }
    Viewer::PlayerRequest local_player;
    local_player.id = 1;
    local_player.name = "LocalPlayer";
    local_player.locally_controlled = true;
    local_player.primary_view = true;
    if (!game.AdoptOrAddPlayer(*scene, local_player))
    {
        // Geometry-only viewer scenes still need a camera controller. Seed a
        // transient player at the authored active entity, or just above the
        // origin when the scene has no active viewpoint.
        Viewer::PlayerRuntimeConfig fallback;
        if (const CEngine::Scene::Entity *active = scene->GetEntity(scene->Settings().active_entity))
        {
            fallback.transform = active->GetTransform();
        }
        else
        {
            fallback.transform.position = {0.0f, 0.0f, 1.65f};
            fallback.transform.UpdateWorldMatrix();
        }
        scene->CreateEntity<Viewer::PlayerEntity>("RuntimePlayer", actions, fallback);
        if (!game.AdoptOrAddPlayer(*scene, std::move(local_player)))
        {
            std::cerr << "Failed to create the viewer player.\n";
            return 1;
        }
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
    context.animations = &animations;
    context.rendering = &renderer;
    context.input = &input;
    context.ui = &ui;
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
    Viewer::PlayerEntity *player = game.PrimaryViewPlayer(*scene);
    if (!ConfigureTuningUi(ui, tuning_menu, renderer, player))
    {
        std::cerr << "Failed to configure the viewer tuning UI.\n";
        return 1;
    }
    bool ui_input_mode = false;
    bool show_tuning_panel = false;
    bool tab_was_down = false;
    bool shift_was_down = false;
#endif
    bool game_started = false;
    FpsDisplay fps_display;
    input.SetPointerCaptured(false);
    PlatformEventTargets event_targets{&input};
    double previous_time = window.TimeSeconds();
    double simulation_accumulator = 0.0;
    constexpr double FixedDelta = 1.0 / 60.0;
    constexpr int MaxStepsPerFrame = 4;
    while (!window.ShouldClose())
    {
        window.PollEvents(&ProcessPlatformEvent, &event_targets);
        const glm::ivec2 current_size = window.DrawableSize();
        const int current_width = current_size.x;
        const int current_height = current_size.y;
        if (current_width <= 0 || current_height <= 0)
        {
            window.WaitEvents(100, &ProcessPlatformEvent, &event_targets);
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
        ui.Update(input);
        for (const CEngine::UI::UiEvent &event : ui.DrainEvents())
        {
            if (event.screen == start_menu && event.action == "start_game")
            {
                game_started = true;
                ui.Hide(start_menu);
                input.SetPointerCaptured(true);
                previous_time = window.TimeSeconds();
                simulation_accumulator = 0.0;
            }
#ifdef CENGINE_ENABLE_OPENGL
            else if (event.screen == tuning_menu)
            {
                ApplyTuningEvent(event, ui, tuning_menu, renderer, player);
            }
#endif
        }
        if (input.IsDown(CEngine::Input::Key::Escape))
        {
            window.RequestClose();
            continue;
        }

#ifdef CENGINE_ENABLE_OPENGL
        const bool tab_down = input.IsDown(CEngine::Input::Key::Tab);
        if (game_started && tab_down && !tab_was_down)
        {
            ui_input_mode = !ui_input_mode;
            input.SetPointerCaptured(!ui_input_mode);
        }
        tab_was_down = tab_down;
        const bool shift_down =
            input.IsDown(CEngine::Input::Key::LeftShift) || input.IsDown(CEngine::Input::Key::RightShift);
        if (game_started && shift_down && !shift_was_down)
        {
            show_tuning_panel = !show_tuning_panel;
            ui_input_mode = show_tuning_panel;
            if (show_tuning_panel)
            {
                ui.Show(tuning_menu);
            }
            else
            {
                ui.Hide(tuning_menu);
            }
            input.SetPointerCaptured(!ui_input_mode);
        }
        shift_was_down = shift_down;
        if (!game_started || ui_input_mode)
        {
            input.Set(actions.move_forward, 0.0f);
            input.Set(actions.move_right, 0.0f);
            input.Set(actions.look_yaw, 0.0f);
            input.Set(actions.look_pitch, 0.0f);
            input.Set(actions.sprint, 0.0f);
            input.Set(actions.jump, 0.0f);
        }
#endif

        if (game_started)
        {
            simulation_accumulator += delta_seconds;
        }
        int simulation_steps = 0;
        while (game_started && simulation_accumulator >= FixedDelta && simulation_steps < MaxStepsPerFrame)
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
        audio.Update();
#endif

        fps_display.Update(ui, hud, delta_seconds);
        renderer.SetUiFrame(ui.Compose());
        renderer.Render();
#ifdef CENGINE_ENABLE_OPENGL
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
    window_desc.maximized = true;
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

    const int result = RunScene(window, scene_path, project_root, renderer);
    renderer.Shutdown();
    window.Shutdown();
    return result;
}
