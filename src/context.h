//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/context.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_CONTEXT_H
#define CENGINE_CONTEXT_H

namespace CEngine::Assets
{
class Store;
}
namespace CEngine::Audio
{
class AudioSystem;
}
namespace CEngine::Animations
{
class AnimationSystem;
}
namespace CEngine::Input
{
class InputSystem;
}
namespace CEngine::Renderer
{
class RenderSystem;
}
namespace CEngine::Scene
{
class Scene;
}
class PhysicsSystem;

namespace CEngine
{

/**
 * @brief TODO: Describe Context.
 */
struct Context
{
    Assets::Store *assets = nullptr;
    Audio::AudioSystem *audio = nullptr;
    Animations::AnimationSystem *animations = nullptr;
    Scene::Scene *scene = nullptr;
    Renderer::RenderSystem *rendering = nullptr;
    PhysicsSystem *physics = nullptr;
    Input::InputSystem *input = nullptr;
};

} // namespace CEngine

#endif
