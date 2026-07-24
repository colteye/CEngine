//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/scene/scene.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_SCENE_SCENE_H
#define CENGINE_SCENE_SCENE_H

#include "assets/store.h"
#include "context.h"
#include "entity/entity.h"
#include "physics/physics_types.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

namespace CEngine::Scene
{

/**
 * @brief TODO: Describe EntityConnection.
 */
struct EntityConnection
{
    EntityHandle source;
    EntityHandle target;
    std::string event;
    std::string action;
    std::string parameter;
    float delay_seconds = 0.0f;
    std::uint32_t times_to_fire = 0;
    std::uint32_t times_fired = 0;
};

/**
 * @brief TODO: Describe SceneSettings.
 */
struct SceneSettings
{
    glm::vec3 ambient_color = glm::vec3(0.0f);
    float exposure = 1.0f;
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
    EntityHandle active_entity;
};

/**
 * @brief TODO: Describe Scene.
 */
class Scene
{
  public:
    /**
     * @brief TODO: Describe Scene.
     */
    Scene() = default;
    /**
     * @brief TODO: Describe ~Scene.
     */
    ~Scene();
    /**
     * @brief TODO: Describe Scene.
     */
    Scene(const Scene &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Scene &operator=(const Scene &) = delete;
    /**
     * @brief TODO: Describe Scene.
     */
    Scene(Scene &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Scene &operator=(Scene &&) = delete;

    /**
     * @brief TODO: Describe Reserve.
     *
     * @param entity_count TODO: Describe this parameter.
     */
    void Reserve(std::size_t entity_count);
    /**
     * @brief TODO: Describe AddEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @param name TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    Entity &AddEntity(std::unique_ptr<Entity> entity, std::string_view name = {});

    /**
     * @brief TODO: Describe CreateEntity.
     *
     * @tparam EntityType TODO: Describe this template parameter.
     * @tparam Args TODO: Describe this template parameter.
     * @param name TODO: Describe this parameter.
     * @param args TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    template <typename EntityType, typename... Args>
    EntityType &CreateEntity(std::string_view name = {}, Args &&...args)
    {
        static_assert(std::is_base_of_v<Entity, EntityType>);
        return static_cast<EntityType &>(AddEntity(std::make_unique<EntityType>(std::forward<Args>(args)...), name));
    }

    /**
     * @brief TODO: Describe DestroyEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool DestroyEntity(EntityHandle entity);
    /**
     * @brief TODO: Describe IsAlive.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsAlive(EntityHandle entity) const;
    /**
     * @brief TODO: Describe GetEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    Entity *GetEntity(EntityHandle entity);
    /**
     * @brief TODO: Describe GetEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Entity *GetEntity(EntityHandle entity) const;
    /**
     * @brief TODO: Describe Entities.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<std::unique_ptr<Entity>> &Entities() const
    {
        return entities_;
    }
    /**
     * @brief TODO: Describe EntityCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t EntityCount() const
    {
        return live_count_;
    }
    /**
     * @brief TODO: Describe Connections.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<EntityConnection> &Connections() const
    {
        return connections_;
    }
    /**
     * @brief TODO: Describe AddConnection.
     *
     * @param connection TODO: Describe this parameter.
     */
    void AddConnection(EntityConnection connection);
    bool Emit(EntityHandle source, std::string_view event, EntityHandle activator = {},
              std::string_view parameter = {});
    bool DispatchInput(EntityHandle target, std::string_view input, EntityHandle source = {},
                       EntityHandle activator = {}, std::string_view parameter = {});
    [[nodiscard]] std::size_t DroppedInputCount() const
    {
        return dropped_inputs_;
    }
    std::size_t FindByName(std::string_view name, EntityHandle *entities, std::size_t capacity) const;
    void RegisterPhysicsBody(PhysicsBodyHandle body, EntityHandle owner);
    void UnregisterPhysicsBody(PhysicsBodyHandle body);
    [[nodiscard]] EntityHandle PhysicsBodyOwner(PhysicsBodyHandle body) const;
    void RegisterPhysicsCharacter(PhysicsCharacterHandle character, EntityHandle owner);
    void UnregisterPhysicsCharacter(PhysicsCharacterHandle character);
    [[nodiscard]] EntityHandle PhysicsCharacterOwner(PhysicsCharacterHandle character) const;
    /**
     * @brief TODO: Describe Settings.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const SceneSettings &Settings() const
    {
        return settings_;
    }
    /**
     * @brief TODO: Describe SetSettings.
     *
     * @param settings TODO: Describe this parameter.
     */
    void SetSettings(SceneSettings settings)
    {
        settings_ = settings;
    }
    bool SetActiveEntity(EntityHandle entity);
    /**
     * @brief TODO: Describe Activate.
     *
     * @param context TODO: Describe this parameter.
     */
    void Activate(Context &context);
    /**
     * @brief TODO: Describe Update.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(Context &context, float delta_seconds);
    /**
     * @brief TODO: Describe Stop.
     */
    void Stop() noexcept;

    /**
     * @brief TODO: Describe SetAssetReferences.
     *
     * @param references TODO: Describe this parameter.
     */
    void SetAssetReferences(std::vector<Assets::Reference> references);
    /**
     * @brief TODO: Describe AssetReference.
     *
     * @param index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Assets::Reference *AssetReference(std::uint32_t index) const;
    /**
     * @brief TODO: Describe AssetReferenceCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t AssetReferenceCount() const
    {
        return asset_references_.size();
    }
    /**
     * @brief TODO: Describe AppendAuxiliaryAsset.
     *
     * @param asset TODO: Describe this parameter.
     */
    void AppendAuxiliaryAsset(const Assets::Reference &asset);
    /**
     * @brief TODO: Describe AuxiliaryAsset.
     *
     * @param index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Assets::Reference *AuxiliaryAsset(std::uint32_t index) const;
    /**
     * @brief TODO: Describe AuxiliaryAssetCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t AuxiliaryAssetCount() const
    {
        return auxiliary_assets_.size();
    }

  private:
    struct PendingInput
    {
        EntityHandle target;
        EntityHandle source;
        EntityHandle activator;
        std::string input;
        std::string parameter;
        double delivery_time = 0.0;
        std::uint64_t sequence = 0;
    };

    struct PhysicsOwner
    {
        std::uint32_t generation = 0;
        EntityHandle entity;
    };

    void ProcessPhysicsContacts(Context &context);
    void ProcessPendingInputs(Context &context);
    void SynchronizeAudioListener(Context &context, float delta_seconds);

    Context active_context_;
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<Assets::Reference> asset_references_;
    std::vector<Assets::Reference> auxiliary_assets_;
    std::vector<Entity *> started_entities_;
    std::vector<EntityConnection> connections_;
    std::vector<PendingInput> pending_inputs_;
    std::vector<PhysicsOwner> body_owners_;
    std::vector<PhysicsOwner> character_owners_;
    SceneSettings settings_;
    std::size_t live_count_ = 0;
    std::size_t dropped_inputs_ = 0;
    std::uint64_t next_input_sequence_ = 1;
    double scene_time_ = 0.0;
#ifdef CENGINE_ENABLE_AUDIO
    glm::vec3 previous_listener_position_ = glm::vec3(0.0f);
    bool listener_position_valid_ = false;
#endif
    bool active_ = false;
};

} // namespace CEngine::Scene

#endif
