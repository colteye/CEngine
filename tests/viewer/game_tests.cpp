//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/viewer/game_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "context.h"
#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "input/actions.h"
#include "input/input_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{

/**
 * @brief TODO: Describe Expect.
 *
 * @param value TODO: Describe this parameter.
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

/**
 * @brief TODO: Describe Near.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

} // namespace

/**
 * @brief TODO: Describe main.
 *
 * @return TODO: Describe the return value.
 */
int main()
{
    CEngine::Input::InputSystem input;
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    CEngine::Entities::EntityFactory factory;
    const bool registered = factory.Register<Viewer::PlayerEntity, Viewer::Generated::Player>("player", actions);
    if (!Expect(registered, "viewer game should register its player entity"))
    {
        return 1;
    }

    CEngine::Scene::Scene scene;
    auto &player = scene.CreateEntity<Viewer::PlayerEntity>("Player", actions);
    player.vertical_fov_radians = 0.9f;
    player.near_clip = 0.2f;
    player.far_clip = 750.0f;

    CEngine::Renderer::RenderSystem rendering;
    CEngine::Context context;
    context.rendering = &rendering;
    context.input = &input;
    scene.Activate(context);
    input.Set(actions.move_forward, 1.0f);
    scene.Update(context, 1.0f);

    const CEngine::Renderer::Camera &camera = rendering.ActiveCamera();
    return Expect(player.GetTransform().position != glm::vec3(0.0f),
                  "viewer player should apply its registered movement action") &&
                   Expect(camera.position == player.GetTransform().position,
                          "viewer player should publish its camera position") &&
                   Expect(Near(camera.vertical_fov_radians, player.vertical_fov_radians) &&
                              Near(camera.near_clip, player.near_clip) && Near(camera.far_clip, player.far_clip),
                          "viewer player should publish its camera lens")
               ? 0
               : 1;
}
