//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/prop_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_PROP_ENTITY_H
#define CENGINE_ENTITY_PROP_ENTITY_H

#include "animation/animation_system.h"
#include "assets/animation_asset.h"
#include "assets/skeleton_asset.h"
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

/**
 * @brief TODO: Describe PropEntity.
 */
class PropEntity final : public Scene::Entity, public Generated::EngineEntities::PropProperties
{
  public:
    /**
     * @brief TODO: Describe Classname.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    /**
     * @brief TODO: Describe Initialize.
     *
     * @param context TODO: Describe this parameter.
     */
    void Initialize(Context &context) override;
    /**
     * @brief TODO: Describe Update.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(Context &context, float delta_seconds) override;
    /**
     * @brief TODO: Describe LateUpdate.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void LateUpdate(Context &context, float delta_seconds) override;
    /**
     * @brief TODO: Describe Shutdown.
     *
     * @param context TODO: Describe this parameter.
     */
    void Shutdown(Context &context) override;
    /**
     * @brief TODO: Describe PhysicsBody.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] PhysicsBodyHandle PhysicsBody() const
    {
        return physics_body_;
    }

  private:
    /**
     * @brief TODO: Describe RegisterMeshInstance.
     *
     * @param context TODO: Describe this parameter.
     * @param mesh TODO: Describe this parameter.
     * @param material TODO: Describe this parameter.
     * @param lightmap TODO: Describe this parameter.
     */
    void RegisterMeshInstance(Context &context, std::shared_ptr<const Renderer::Mesh> mesh,
                              std::shared_ptr<const Renderer::Material> material,
                              std::shared_ptr<const Renderer::Texture> lightmap);
    /**
     * @brief TODO: Describe BuildMeshInstanceFlags.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint32_t BuildMeshInstanceFlags() const;
    void UpdatePresentation(Context &context);

    Renderer::MeshInstanceHandle renderer_mesh_instance_;
    PhysicsBodyHandle physics_body_;
    std::shared_ptr<const Assets::Skeleton> skeleton_asset_;
    std::shared_ptr<const Assets::Animation> animation_asset_;
    Animations::AnimationInstanceHandle animation_instance_;
};

} // namespace CEngine::Entities

#endif
