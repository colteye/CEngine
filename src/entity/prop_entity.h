#ifndef CENGINE_ENTITY_PROP_ENTITY_H
#define CENGINE_ENTITY_PROP_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "physics/physics_types.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/mesh_instance.h"
#include "renderer/texture.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities
{

class PropEntity final : public Scene::Entity, public Generated::EngineEntities::PropProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void LateUpdate(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;
    [[nodiscard]] PhysicsBodyHandle PhysicsBody() const
    {
        return physics_body_;
    }

  private:
    void RegisterMeshInstance(Context &context, std::shared_ptr<const Renderer::Mesh> mesh,
                              std::shared_ptr<const Renderer::Material> material,
                              std::shared_ptr<const Renderer::Texture> lightmap);
    [[nodiscard]] std::uint32_t BuildMeshInstanceFlags() const;

    Renderer::MeshInstanceHandle renderer_mesh_instance_;
    PhysicsBodyHandle physics_body_;
};

} // namespace CEngine::Entities

#endif
