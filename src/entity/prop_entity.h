#ifndef CENGINE_ENTITY_PROP_ENTITY_H
#define CENGINE_ENTITY_PROP_ENTITY_H

#include "entity/entity.h"
#include "engine/engine_entities.generated.h"
#include "physics/physics_types.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/renderable.h"
#include "renderer/texture.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities {

class PropEntity final : public Scene::Entity,
    public Generated::EngineEntities::PropProperties {
public:
    std::string_view Classname() const override;
    void Initialize(EngineContext& context) override;
    void LateUpdate(EngineContext& context, float delta_seconds) override;
    void Shutdown(EngineContext& context) override;

private:
    void RegisterRenderable(EngineContext& context);
    std::uint32_t RenderableFlags() const;

    Renderer::Material renderer_material_;
    Renderer::Mesh renderer_mesh_;
    std::shared_ptr<const Renderer::Texture> renderer_lightmap_;
    Renderer::RenderableHandle renderer_renderable_;
    PhysicsBodyHandle physics_body_ = kInvalidPhysicsBodyHandle;
    bool material_registered_ = false;
    bool lightmap_registered_ = false;
};

} // namespace CEngine::Entities

#endif
