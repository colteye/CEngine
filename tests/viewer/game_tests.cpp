#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "engine_context.h"
#include "input/actions.h"
#include "input/input_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace {

bool Expect(bool value, const char* message)
{
    if (!value) std::cerr << message << '\n';
    return value;
}

bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

} // namespace

int main()
{
    CEngine::Input::InputSystem input;
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    CEngine::Entities::EntityFactory factory;
    const bool registered =
        factory.Register<Viewer::PlayerEntity, Viewer::Generated::Player>(
            "player", actions);
    if (!Expect(registered, "viewer game should register its player entity"))
        return 1;

    CEngine::Scene::Scene scene;
    auto& player =
        scene.CreateEntity<Viewer::PlayerEntity>("Player", actions);
    player.vertical_fov_radians = 0.9f;
    player.near_clip = 0.2f;
    player.far_clip = 750.0f;

    CEngine::Renderer::RenderSystem rendering;
    CEngine::EngineContext context;
    context.rendering = &rendering;
    context.input = &input;
    scene.Activate(context);
    input.Set(actions.move_forward, 1.0f);
    scene.Update(context, 1.0f);

    const CEngine::Renderer::Camera& camera = rendering.ActiveCamera();
    return Expect(player.GetTransform().position != glm::vec3(0.0f),
            "viewer player should apply its registered movement action") &&
        Expect(camera.position == player.GetTransform().position,
            "viewer player should publish its camera position") &&
        Expect(Near(camera.vertical_fov_radians, player.vertical_fov_radians) &&
                Near(camera.near_clip, player.near_clip) &&
                Near(camera.far_clip, player.far_clip),
            "viewer player should publish its camera lens") ? 0 : 1;
}
