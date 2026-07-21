#ifndef CENGINE_ENTITY_ENTITY_H
#define CENGINE_ENTITY_ENTITY_H

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace CEngine::Scene {

class Scene;
struct EntityEvent;

struct EntityId {
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index = InvalidIndex;
    std::uint32_t generation = 0;
    constexpr explicit operator bool() const { return index != InvalidIndex; }
};

constexpr bool operator==(EntityId left, EntityId right)
{ return left.index == right.index && left.generation == right.generation; }
constexpr bool operator!=(EntityId left, EntityId right) { return !(left == right); }

struct EntityContext {
    Scene& scene;
};

enum EntityFlags : std::uint32_t {
    EntityEnabled = 1u << 0u,
};

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 world_matrix = glm::mat4(1.0f);
    bool dirty = true;

    void UpdateWorldMatrix();
};

class Entity {
public:
    virtual ~Entity() = default;
    virtual std::string_view Classname() const = 0;

    virtual void OnLoaded(EntityContext&) {}
    virtual void OnSceneReady(EntityContext&) {}
    virtual void OnEvent(EntityContext&, const EntityEvent&) {}
    virtual void OnStop(EntityContext&) {}

    EntityId Id() const { return id_; }
    const std::string& Name() const { return name_; }
    std::uint32_t Flags() const { return flags_; }
    void SetFlags(std::uint32_t flags) { flags_ = flags; }
    bool Enabled() const { return (flags_ & EntityEnabled) != 0; }
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

protected:
    explicit Entity(std::uint32_t flags = EntityEnabled) : flags_(flags) {}

private:
    friend class Scene;
    EntityId id_;
    std::string name_;
    std::uint32_t flags_ = EntityEnabled;
    Transform transform_;
};

} // namespace CEngine::Scene

#endif
