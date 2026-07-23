#ifndef CENGINE_ENGINE_CONTEXT_H
#define CENGINE_ENGINE_CONTEXT_H

namespace CEngine::Assets { class AssetDatabase; }
namespace CEngine::Input { class InputSystem; }
namespace CEngine::Renderer { class RenderSystem; }
namespace CEngine::Scene { class Scene; }
class PhysicsSystem;

namespace CEngine {

struct EngineContext {
    Assets::AssetDatabase* assets = nullptr;
    Scene::Scene* scene = nullptr;
    Renderer::RenderSystem* rendering = nullptr;
    PhysicsSystem* physics = nullptr;
    Input::InputSystem* input = nullptr;
};

} // namespace CEngine

#endif
