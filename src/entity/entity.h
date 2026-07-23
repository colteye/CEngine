#ifndef CENGINE_ENTITY_ENTITY_H
#define CENGINE_ENTITY_ENTITY_H

#include "foundation/slot_handle.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace CEngine
{
struct EngineContext;
}
namespace CEngine::Scene
{

class Scene;

struct EntitySlotTag;
using EntityId = SlotHandle<EntitySlotTag>;

inline constexpr std::uint32_t EntityEnabled = 1u << 0u;

struct Transform
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 world_matrix = glm::mat4(1.0f);
    bool dirty = true;

    void UpdateWorldMatrix();
};

class Entity
{
  public:
    virtual ~Entity() = default;
    [[nodiscard]] virtual std::string_view Classname() const = 0;

    virtual void Initialize(EngineContext & /*unused*/)
    {
    }
    virtual void Update(EngineContext & /*unused*/, float /*unused*/)
    {
    }
    virtual void LateUpdate(EngineContext & /*unused*/, float /*unused*/)
    {
    }
    virtual void Shutdown(EngineContext & /*unused*/)
    {
    }

    [[nodiscard]] EntityId Id() const
    {
        return id_;
    }
    [[nodiscard]] const std::string &Name() const
    {
        return name_;
    }
    [[nodiscard]] std::uint32_t Flags() const
    {
        return flags_;
    }
    void SetFlags(std::uint32_t flags)
    {
        flags_ = flags;
    }
    [[nodiscard]] bool Enabled() const
    {
        return (flags_ & EntityEnabled) != 0;
    }
    Transform &GetTransform()
    {
        return transform_;
    }
    [[nodiscard]] const Transform &GetTransform() const
    {
        return transform_;
    }

  protected:
    explicit Entity(std::uint32_t flags = EntityEnabled) : flags_(flags)
    {
    }

  private:
    friend class Scene;
    EntityId id_;
    std::string name_;
    std::uint32_t flags_ = EntityEnabled;
    Transform transform_;
};

} // namespace CEngine::Scene

#endif
