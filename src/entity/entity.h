//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_ENTITY_H
#define CENGINE_ENTITY_ENTITY_H

#include "handle.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace CEngine
{
struct Context;
}
namespace CEngine::Scene
{

class Scene;

struct EntitySlotTag;
using EntityHandle = Handle<EntitySlotTag>;

inline constexpr std::uint32_t EntityEnabled = 1u << 0u;

struct EntityInput
{
    EntityHandle source;
    EntityHandle activator;
    std::string_view name;
    std::string_view parameter;
};

enum class EntityContactPhase : std::uint8_t
{
    Begin,
    Persist,
    End,
};

struct EntityContact
{
    EntityHandle other;
    EntityContactPhase phase = EntityContactPhase::Begin;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    bool sensor = false;
    bool other_is_character = false;
};

/**
 * @brief TODO: Describe Transform.
 */
struct Transform
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 world_matrix = glm::mat4(1.0f);
    bool dirty = true;

    /**
     * @brief TODO: Describe UpdateWorldMatrix.
     */
    void UpdateWorldMatrix();
};

/**
 * @brief TODO: Describe Entity.
 */
class Entity
{
  public:
    /**
     * @brief TODO: Describe ~Entity.
     */
    virtual ~Entity() = default;
    /**
     * @brief TODO: Describe Entity.
     */
    Entity(const Entity &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Entity &operator=(const Entity &) = delete;
    /**
     * @brief TODO: Describe Entity.
     */
    Entity(Entity &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Entity &operator=(Entity &&) = delete;

    /**
     * @brief TODO: Describe Classname.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::string_view Classname() const = 0;

    [[nodiscard]] virtual bool AcceptsInput(std::string_view input) const;
    [[nodiscard]] virtual bool HasOutput(std::string_view output) const;
    virtual bool HandleInput(Context &context, const EntityInput &input);
    virtual void OnPhysicsContact(Context & /*unused*/, const EntityContact & /*unused*/)
    {
    }

    /**
     * @brief TODO: Describe Initialize.
     */
    virtual void Initialize(Context & /*unused*/)
    {
    }
    /**
     * @brief TODO: Describe Update.
     */
    virtual void Update(Context & /*unused*/, float /*unused*/)
    {
    }
    /**
     * @brief TODO: Describe LateUpdate.
     */
    virtual void LateUpdate(Context & /*unused*/, float /*unused*/)
    {
    }
    /**
     * @brief TODO: Describe Shutdown.
     */
    virtual void Shutdown(Context & /*unused*/)
    {
    }

    /**
     * @brief TODO: Describe GetHandle.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] EntityHandle GetHandle() const
    {
        return id_;
    }
    /**
     * @brief TODO: Describe Name.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::string &Name() const
    {
        return name_;
    }
    /**
     * @brief TODO: Describe Flags.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint32_t Flags() const
    {
        return flags_;
    }
    /**
     * @brief TODO: Describe SetFlags.
     *
     * @param flags TODO: Describe this parameter.
     */
    void SetFlags(std::uint32_t flags)
    {
        flags_ = flags;
    }
    bool SetEnabled(Context &context, bool enabled);
    /**
     * @brief TODO: Describe Enabled.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool Enabled() const
    {
        return (flags_ & EntityEnabled) != 0;
    }
    /**
     * @brief TODO: Describe GetTransform.
     *
     * @return TODO: Describe the return value.
     */
    Transform &GetTransform()
    {
        return transform_;
    }
    /**
     * @brief TODO: Describe GetTransform.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Transform &GetTransform() const
    {
        return transform_;
    }

  protected:
    /**
     * @brief TODO: Describe Entity.
     *
     * @param flags TODO: Describe this parameter.
     */
    explicit Entity(std::uint32_t flags = EntityEnabled) : flags_(flags)
    {
    }

    virtual void OnEnabledChanged(Context & /*unused*/, bool /*unused*/)
    {
    }

  private:
    friend class Scene;
    EntityHandle id_;
    std::string name_;
    std::uint32_t flags_ = EntityEnabled;
    Transform transform_;
};

} // namespace CEngine::Scene

#endif
