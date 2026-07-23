#include "physics/physics_system.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace
{
namespace Layers
{
constexpr std::uint8_t Count = 32;
constexpr JPH::ObjectLayer MovingBit = Count;
constexpr JPH::ObjectLayer NumLayers = Count * 2;

std::uint8_t CollisionLayer(JPH::ObjectLayer layer)
{
    return static_cast<std::uint8_t>(layer & (Count - 1));
}

bool IsMoving(JPH::ObjectLayer layer)
{
    return (layer & MovingBit) != 0;
}
} // namespace Layers

namespace BroadPhaseLayers
{
const JPH::BroadPhaseLayer NonMoving(0);
const JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint NumLayers = 2;
} // namespace BroadPhaseLayers

JPH::Vec3 ToJoltVec3(const glm::vec3 &value)
{
    return {value.x, value.y, value.z};
}

JPH::RVec3 ToJoltRVec3(const glm::vec3 &value)
{
    return {value.x, value.y, value.z};
}

JPH::Quat ToJoltQuat(const glm::quat &value)
{
    return {value.x, value.y, value.z, value.w};
}

glm::vec3 ToGlmRVec3(const JPH::RVec3 &value)
{
    return {value.GetX(), value.GetY(), value.GetZ()};
}

glm::vec3 ToGlmVec3(const JPH::Vec3 &value)
{
    return {value.GetX(), value.GetY(), value.GetZ()};
}

glm::quat ToGlmQuat(const JPH::Quat &value)
{
    return {value.GetW(), value.GetX(), value.GetY(), value.GetZ()};
}

JPH::EMotionType ToJoltMotionType(PhysicsMotionType motion_type)
{
    switch (motion_type)
    {
    case PhysicsMotionType::Static:
        return JPH::EMotionType::Static;
    case PhysicsMotionType::Dynamic:
        return JPH::EMotionType::Dynamic;
    case PhysicsMotionType::Kinematic:
        return JPH::EMotionType::Kinematic;
    }

    return JPH::EMotionType::Static;
}

JPH::ObjectLayer ToJoltLayer(PhysicsMotionType motion_type)
{
    return motion_type == PhysicsMotionType::Static ? 0 : Layers::MovingBit;
}

JPH::ObjectLayer ToJoltLayer(PhysicsMotionType motion_type, std::uint8_t collision_layer)
{
    return static_cast<JPH::ObjectLayer>(ToJoltLayer(motion_type) | collision_layer);
}

bool Finite(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Finite(const glm::quat &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w) &&
           glm::dot(value, value) > 1.0e-12f;
}

bool Positive(const glm::vec3 &value)
{
    return Finite(value) && value.x > 0.0f && value.y > 0.0f && value.z > 0.0f;
}

bool ValidateShape(const PhysicsShape &shape, std::uint32_t depth = 0)
{
    if (depth > 8 || !Finite(shape.local_position) || !Finite(shape.local_rotation))
    {
        return false;
    }
    const bool no_geometry =
        shape.vertices.empty() && shape.indices.empty() && shape.heights.empty() && shape.samples_per_side == 0;
    switch (shape.type)
    {
    case PhysicsShapeType::Box:
        return Positive(shape.half_extents) && no_geometry && shape.children.empty();
    case PhysicsShapeType::Sphere:
        return std::isfinite(shape.radius) && shape.radius > 0.0f && no_geometry && shape.children.empty();
    case PhysicsShapeType::Capsule:
    case PhysicsShapeType::Cylinder:
        return std::isfinite(shape.radius) && shape.radius > 0.0f && std::isfinite(shape.half_height) &&
               shape.half_height > 0.0f && no_geometry && shape.children.empty();
    case PhysicsShapeType::Plane:
        return Finite(shape.half_extents) && shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               no_geometry && shape.children.empty();
    case PhysicsShapeType::ConvexHull:
        if (shape.vertices.size() < 4 || shape.vertices.size() > 65536)
        {
            return false;
        }
        for (const glm::vec3 &vertex : shape.vertices)
            if (!Finite(vertex))
                return false;
        return shape.indices.empty() && shape.heights.empty() && shape.samples_per_side == 0 && shape.children.empty();
    case PhysicsShapeType::TriangleMesh:
        if (shape.vertices.size() < 3 || shape.indices.empty() || shape.indices.size() % 3 != 0)
        {
            return false;
        }
        for (const glm::vec3 &vertex : shape.vertices)
            if (!Finite(vertex))
                return false;
        for (std::uint32_t index : shape.indices)
            if (index >= shape.vertices.size())
                return false;
        return shape.heights.empty() && shape.samples_per_side == 0 && shape.children.empty();
    case PhysicsShapeType::HeightField:
        if (shape.samples_per_side < 2 ||
            shape.heights.size() != static_cast<std::size_t>(shape.samples_per_side) * shape.samples_per_side ||
            !Finite(shape.height_field_offset) || !Positive(shape.height_field_scale))
        {
            return false;
        }
        for (float height : shape.heights)
            if (!std::isfinite(height))
                return false;
        return shape.vertices.empty() && shape.indices.empty() && shape.children.empty();
    case PhysicsShapeType::Compound:
        return !(!no_geometry || shape.children.empty() || shape.children.size() > 1024);
    }
    return false;
}

bool IsMovableShape(const PhysicsShape &shape)
{
    return !(shape.type == PhysicsShapeType::TriangleMesh || shape.type == PhysicsShapeType::HeightField ||
             shape.type == PhysicsShapeType::Plane);
}

JPH::RefConst<JPH::Shape> ShapeResult(JPH::ShapeSettings::ShapeResult result)
{
    if (!result.IsValid())
    {
        if (result.HasError())
        {
            std::cerr << "Jolt rejected collision shape: " << result.GetError() << '\n';
        }
        return nullptr;
    }
    return result.Get();
}

JPH::RefConst<JPH::Shape> BuildShape(const PhysicsShape &shape, bool apply_local_transform = true)
{
    JPH::RefConst<JPH::Shape> result;
    switch (shape.type)
    {
    case PhysicsShapeType::Box:
        result = new JPH::BoxShape(ToJoltVec3(shape.half_extents));
        break;
    case PhysicsShapeType::Sphere:
        result = new JPH::SphereShape(shape.radius);
        break;
    case PhysicsShapeType::Capsule:
        result = new JPH::CapsuleShape(shape.half_height, shape.radius);
        result =
            ShapeResult(JPH::RotatedTranslatedShapeSettings(
                            JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), result)
                            .Create());
        break;
    case PhysicsShapeType::Cylinder:
        result = new JPH::CylinderShape(shape.half_height, shape.radius);
        result =
            ShapeResult(JPH::RotatedTranslatedShapeSettings(
                            JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), result)
                            .Create());
        break;
    case PhysicsShapeType::Plane:
        result = new JPH::PlaneShape(JPH::Plane(JPH::Vec3::sAxisZ(), 0.0f), nullptr,
                                     std::max(shape.half_extents.x, shape.half_extents.y));
        break;
    case PhysicsShapeType::ConvexHull: {
        JPH::Array<JPH::Vec3> points;
        points.reserve(shape.vertices.size());
        for (const glm::vec3 &point : shape.vertices)
            points.push_back(ToJoltVec3(point));
        result = ShapeResult(JPH::ConvexHullShapeSettings(points).Create());
        break;
    }
    case PhysicsShapeType::TriangleMesh: {
        JPH::VertexList vertices;
        JPH::IndexedTriangleList triangles;
        vertices.reserve(shape.vertices.size());
        for (const glm::vec3 &vertex : shape.vertices)
            vertices.emplace_back(vertex.x, vertex.y, vertex.z);
        triangles.reserve(shape.indices.size() / 3);
        for (std::size_t index = 0; index < shape.indices.size(); index += 3)
        {
            triangles.emplace_back(shape.indices[index], shape.indices[index + 1], shape.indices[index + 2]);
        }
        result = ShapeResult(JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create());
        break;
    }
    case PhysicsShapeType::HeightField: {
        const std::uint32_t side = shape.samples_per_side;
        std::vector<float> samples(shape.heights.size());
        for (std::uint32_t y = 0; y < side; ++y)
        {
            for (std::uint32_t x = 0; x < side; ++x)
            {
                samples[(static_cast<std::size_t>(side - 1 - y) * side) + x] =
                    shape.heights[(static_cast<std::size_t>(y) * side) + x];
            }
        }
        const glm::vec3 &offset = shape.height_field_offset;
        const glm::vec3 &scale = shape.height_field_scale;
        const JPH::Vec3 jolt_offset(offset.x, offset.z, -(offset.y + ((side - 1) * scale.y)));
        const JPH::Vec3 jolt_scale(scale.x, scale.z, scale.y);
        result = ShapeResult(JPH::HeightFieldShapeSettings(samples.data(), jolt_offset, jolt_scale, side).Create());
        if (result != nullptr)
            result = ShapeResult(
                JPH::RotatedTranslatedShapeSettings(
                    JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), result)
                    .Create());
        break;
    }
    case PhysicsShapeType::Compound: {
        JPH::StaticCompoundShapeSettings compound;
        for (const PhysicsShape &child : shape.children)
        {
            JPH::RefConst<JPH::Shape> child_shape = BuildShape(child, false);
            if (child_shape == nullptr)
                return nullptr;
            compound.AddShape(ToJoltVec3(child.local_position), ToJoltQuat(glm::normalize(child.local_rotation)),
                              child_shape);
        }
        result = ShapeResult(compound.Create());
        break;
    }
    }

    if (result == nullptr || !apply_local_transform)
        return result;
    const glm::quat rotation = glm::normalize(shape.local_rotation);
    const bool translated = glm::dot(shape.local_position, shape.local_position) > 0.0f;
    const bool rotated = std::abs(rotation.w - 1.0f) > 1.0e-6f || std::abs(rotation.x) > 1.0e-6f ||
                         std::abs(rotation.y) > 1.0e-6f || std::abs(rotation.z) > 1.0e-6f;
    if (translated || rotated)
        result = ShapeResult(
            JPH::RotatedTranslatedShapeSettings(ToJoltVec3(shape.local_position), ToJoltQuat(rotation), result)
                .Create());
    return result;
}

JPH::RefConst<JPH::Shape> BuildCharacterShape(float radius, float height)
{
    if (!std::isfinite(radius) || !std::isfinite(height) || radius <= 0.0f || height <= 2.0f * radius)
    {
        return nullptr;
    }
    const float half_height = (0.5f * height) - radius;
    const JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(half_height, radius);
    return ShapeResult(
        JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0.0f, 0.0f, 0.5f * height),
                                            JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), capsule)
            .Create());
}

struct RawContact
{
    PhysicsContactType type = PhysicsContactType::Begin;
    JPH::BodyID first;
    JPH::BodyID second;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    bool sensor = false;
};

class ContactCollector final : public JPH::ContactListener
{
  public:
    void Reset(std::vector<RawContact> *target, std::size_t capacity, std::uint64_t *dropped)
    {
        target_ = target;
        capacity_ = capacity;
        dropped_ = dropped;
    }

    void OnContactAdded(const JPH::Body &first, const JPH::Body &second, const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &settings) override
    {
        Add(PhysicsContactType::Begin, first.GetID(), second.GetID(), manifold, settings.mIsSensor);
    }

    void OnContactPersisted(const JPH::Body &first, const JPH::Body &second, const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &settings) override
    {
        Add(PhysicsContactType::Persist, first.GetID(), second.GetID(), manifold, settings.mIsSensor);
    }

    void OnContactRemoved(const JPH::SubShapeIDPair &pair) override
    {
        RawContact contact;
        contact.type = PhysicsContactType::End;
        contact.first = pair.GetBody1ID();
        contact.second = pair.GetBody2ID();
        Push(contact);
    }

  private:
    void Add(PhysicsContactType type, JPH::BodyID first, JPH::BodyID second, const JPH::ContactManifold &manifold,
             bool sensor)
    {
        RawContact contact;
        contact.type = type;
        contact.first = first;
        contact.second = second;
        if (!manifold.mRelativeContactPointsOn1.empty())
        {
            contact.position = ToGlmRVec3(manifold.GetWorldSpaceContactPointOn1(0));
        }
        contact.normal = ToGlmVec3(manifold.mWorldSpaceNormal);
        contact.sensor = sensor;
        Push(contact);
    }

    void Push(const RawContact &contact)
    {
        if (target_ != nullptr && target_->size() < capacity_)
        {
            target_->push_back(contact);
        }
        else if (dropped_ != nullptr)
        {
            ++*dropped_;
        }
    }

    std::vector<RawContact> *target_ = nullptr;
    std::size_t capacity_ = 0;
    std::uint64_t *dropped_ = nullptr;
};

class QueryLayerFilter final : public JPH::ObjectLayerFilter
{
  public:
    explicit QueryLayerFilter(std::uint32_t mask) : mask_(mask)
    {
    }

    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override
    {
        return (mask_ & (1u << Layers::CollisionLayer(layer))) != 0;
    }

  private:
    std::uint32_t mask_;
};

class QueryBodyFilter final : public JPH::BodyFilter
{
  public:
    explicit QueryBodyFilter(JPH::BodyID ignored) : ignored_(ignored)
    {
    }

    [[nodiscard]] bool ShouldCollide(const JPH::BodyID &body) const override
    {
        return body != ignored_;
    }

  private:
    JPH::BodyID ignored_;
};
} // namespace

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
  public:
    ObjectLayerPairFilterImpl()
    {
        masks.fill(~0u);
    }

    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override
    {
        if (!Layers::IsMoving(object1) && !Layers::IsMoving(object2))
        {
            return false;
        }
        const std::uint8_t first = Layers::CollisionLayer(object1);
        const std::uint8_t second = Layers::CollisionLayer(object2);
        return (masks[first] & (1u << second)) != 0 && (masks[second] & (1u << first)) != 0;
    }

    std::array<std::uint32_t, Layers::Count> masks;
};

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
  public:
    BroadPhaseLayerInterfaceImpl()
    {
        for (JPH::ObjectLayer layer = 0; layer < Layers::NumLayers; ++layer)
        {
            object_to_broad_phase_[layer] =
                Layers::IsMoving(layer) ? BroadPhaseLayers::Moving : BroadPhaseLayers::NonMoving;
        }
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NumLayers;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        if (layer >= Layers::NumLayers)
        {
            return BroadPhaseLayers::NonMoving;
        }

        return object_to_broad_phase_[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        if (layer == BroadPhaseLayers::NonMoving)
        {
            return "NON_MOVING";
        }
        if (layer == BroadPhaseLayers::Moving)
        {
            return "MOVING";
        }
        return "INVALID";
    }
#endif

  private:
    JPH::BroadPhaseLayer object_to_broad_phase_[Layers::NumLayers]{};
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
  public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
    {
        return Layers::IsMoving(layer1) || layer2 == BroadPhaseLayers::Moving;
    }
};

struct PhysicsSystem::Impl
{
    struct BodySlot
    {
        JPH::BodyID body;
        std::uint32_t generation = 1;
        PhysicsMotionType motion_type = PhysicsMotionType::Static;
    };

    struct ConstraintSlot
    {
        JPH::Ref<JPH::Constraint> constraint{};
        PhysicsBodyHandle first;
        PhysicsBodyHandle second;
        std::uint32_t generation = 1;
    };

    struct CharacterSlot
    {
        JPH::Ref<JPH::CharacterVirtual> character{};
        float radius = 0.0f;
        float height = 0.0f;
        float step_height = 0.0f;
        float stick_to_floor_distance = 0.0f;
        std::uint8_t collision_layer = 0;
        std::uint32_t generation = 1;
    };

    [[nodiscard]] JPH::BodyID GetBodyID(PhysicsBodyHandle handle) const
    {
        if (handle.index >= bodies.size() || bodies[handle.index].generation != handle.generation)
        {
            return {};
        }

        return bodies[handle.index].body;
    }

    [[nodiscard]] PhysicsBodyHandle GetBodyHandle(JPH::BodyID body) const
    {
        const auto found = body_handles.find(body.GetIndexAndSequenceNumber());
        return found != body_handles.end() ? found->second : PhysicsBodyHandle{};
    }

    [[nodiscard]] JPH::Constraint *GetConstraint(PhysicsConstraintHandle handle) const
    {
        return handle.index < constraints.size() && constraints[handle.index].generation == handle.generation
                   ? constraints[handle.index].constraint.GetPtr()
                   : nullptr;
    }

    [[nodiscard]] JPH::CharacterVirtual *GetCharacter(PhysicsCharacterHandle handle) const
    {
        return handle.index < characters.size() && characters[handle.index].generation == handle.generation
                   ? characters[handle.index].character.GetPtr()
                   : nullptr;
    }

    std::unique_ptr<BroadPhaseLayerInterfaceImpl> broad_phase_layer_interface{};
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> object_vs_broad_phase_layer_filter{};
    std::unique_ptr<ObjectLayerPairFilterImpl> object_layer_pair_filter{};
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator{};
    std::unique_ptr<JPH::JobSystemSingleThreaded> job_system{};
    std::unique_ptr<JPH::PhysicsSystem> physics_system{};
    std::vector<BodySlot> bodies{};
    std::vector<std::uint32_t> free_handles{};
    std::vector<ConstraintSlot> constraints{};
    std::vector<std::uint32_t> free_constraint_handles{};
    std::vector<CharacterSlot> characters{};
    std::vector<std::uint32_t> free_character_handles{};
    std::unordered_map<std::uint32_t, PhysicsBodyHandle> body_handles{};
    ContactCollector contact_collector;
    std::vector<RawContact> contact_events{};
    std::size_t max_contact_events = 0;
    std::uint64_t dropped_contact_events = 0;
    std::size_t body_count = 0;
    std::size_t constraint_count = 0;
    std::size_t character_count = 0;
    bool initialized = false;
    bool owns_jolt_runtime = false;
};

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem()
{
    Shutdown();
}

PhysicsSystem::PhysicsSystem(PhysicsSystem &&) noexcept = default;
PhysicsSystem &PhysicsSystem::operator=(PhysicsSystem &&) noexcept = default;

bool PhysicsSystem::Initialize(const PhysicsSystemDesc &desc)
{
    Shutdown();
    impl_ = std::make_unique<Impl>();

    if (JPH::Factory::sInstance == nullptr)
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        impl_->owns_jolt_runtime = true;
    }

    impl_->broad_phase_layer_interface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    impl_->object_vs_broad_phase_layer_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    impl_->object_layer_pair_filter = std::make_unique<ObjectLayerPairFilterImpl>();
    impl_->temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    impl_->job_system = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

    impl_->physics_system = std::make_unique<JPH::PhysicsSystem>();
    impl_->physics_system->Init(desc.max_bodies, 0, desc.max_body_pairs, desc.max_contact_constraints,
                                *impl_->broad_phase_layer_interface, *impl_->object_vs_broad_phase_layer_filter,
                                *impl_->object_layer_pair_filter);
    impl_->physics_system->SetGravity(ToJoltVec3(desc.gravity));
    impl_->max_contact_events = desc.max_contact_events;
    impl_->contact_events.reserve(desc.max_contact_events);
    impl_->contact_collector.Reset(&impl_->contact_events, impl_->max_contact_events, &impl_->dropped_contact_events);
    impl_->physics_system->SetContactListener(&impl_->contact_collector);

    impl_->initialized = true;
    return true;
}

void PhysicsSystem::Shutdown()
{
    if (impl_ == nullptr)
    {
        return;
    }

    if (impl_->physics_system != nullptr)
    {
        for (Impl::CharacterSlot &slot : impl_->characters)
            slot.character = nullptr;
        for (Impl::ConstraintSlot &slot : impl_->constraints)
        {
            if (slot.constraint != nullptr)
            {
                impl_->physics_system->RemoveConstraint(slot.constraint);
                slot.constraint = nullptr;
            }
        }
        JPH::BodyInterface &body_interface = impl_->physics_system->GetBodyInterface();
        for (const Impl::BodySlot &slot : impl_->bodies)
        {
            const JPH::BodyID body_id = slot.body;
            if (!body_id.IsInvalid())
            {
                body_interface.RemoveBody(body_id);
                body_interface.DestroyBody(body_id);
            }
        }
    }

    impl_->bodies.clear();
    impl_->free_handles.clear();
    impl_->constraints.clear();
    impl_->free_constraint_handles.clear();
    impl_->characters.clear();
    impl_->free_character_handles.clear();
    impl_->body_handles.clear();
    impl_->contact_events.clear();
    impl_->body_count = 0;
    impl_->constraint_count = 0;
    impl_->character_count = 0;
    impl_->physics_system.reset();
    impl_->job_system.reset();
    impl_->temp_allocator.reset();
    impl_->object_layer_pair_filter.reset();
    impl_->object_vs_broad_phase_layer_filter.reset();
    impl_->broad_phase_layer_interface.reset();
    impl_->initialized = false;

    if (impl_->owns_jolt_runtime)
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    impl_.reset();
}

void PhysicsSystem::Step(float fixed_delta_time)
{
    if (impl_ == nullptr || !impl_->initialized || impl_->physics_system == nullptr ||
        impl_->temp_allocator == nullptr || impl_->job_system == nullptr)
    {
        return;
    }

    impl_->physics_system->Update(fixed_delta_time, 1, impl_->temp_allocator.get(), impl_->job_system.get());
}

PhysicsBodyHandle PhysicsSystem::CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &source_shape)
{
    if (impl_ == nullptr || !impl_->initialized || impl_->physics_system == nullptr)
    {
        return {};
    }

    if (!ValidateShape(source_shape) || !Finite(desc.position) || !Finite(desc.rotation) || !std::isfinite(desc.mass) ||
        desc.mass <= 0.0f || !std::isfinite(desc.friction) || desc.friction < 0.0f ||
        !std::isfinite(desc.restitution) || desc.restitution < 0.0f || desc.restitution > 1.0f ||
        !Finite(desc.linear_velocity) || !Finite(desc.angular_velocity) || !std::isfinite(desc.linear_damping) ||
        desc.linear_damping < 0.0f || !std::isfinite(desc.angular_damping) || desc.angular_damping < 0.0f ||
        !std::isfinite(desc.gravity_factor) || desc.collision_layer >= Layers::Count ||
        (desc.locked_axes & 0x3fu) == 0x3fu ||
        (desc.motion_type != PhysicsMotionType::Static && !IsMovableShape(source_shape)))
    {
        std::cerr << "Physics rejected invalid body description.\n";
        return {};
    }

    JPH::RefConst<JPH::Shape> shape = BuildShape(source_shape);
    if (shape == nullptr)
    {
        return {};
    }
    JPH::BodyCreationSettings settings(shape, ToJoltRVec3(desc.position), ToJoltQuat(glm::normalize(desc.rotation)),
                                       ToJoltMotionType(desc.motion_type),
                                       ToJoltLayer(desc.motion_type, desc.collision_layer));
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    settings.mIsSensor = desc.sensor;
    settings.mMotionQuality = desc.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
    settings.mAllowSleeping = desc.allow_sleeping;
    settings.mLinearDamping = desc.linear_damping;
    settings.mAngularDamping = desc.angular_damping;
    settings.mGravityFactor = desc.gravity_factor;
    settings.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(static_cast<std::uint8_t>(JPH::EAllowedDOFs::All) &
                                                           static_cast<std::uint8_t>(~desc.locked_axes));
    if (desc.motion_type == PhysicsMotionType::Dynamic && desc.mass > 0.0f)
    {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass;
    }

    JPH::BodyInterface &body_interface = impl_->physics_system->GetBodyInterface();
    JPH::BodyID body_id = body_interface.CreateAndAddBody(settings, desc.motion_type == PhysicsMotionType::Static
                                                                        ? JPH::EActivation::DontActivate
                                                                        : JPH::EActivation::Activate);
    if (body_id.IsInvalid())
    {
        std::cout << "Failed to create Jolt body.\n";
        return {};
    }

    if (desc.motion_type != PhysicsMotionType::Static)
    {
        body_interface.SetLinearVelocity(body_id, ToJoltVec3(desc.linear_velocity));
        body_interface.SetAngularVelocity(body_id, ToJoltVec3(desc.angular_velocity));
    }

    PhysicsBodyHandle handle;
    if (!impl_->free_handles.empty())
    {
        handle.index = impl_->free_handles.back();
        impl_->free_handles.pop_back();
        impl_->bodies[handle.index].body = body_id;
        impl_->bodies[handle.index].motion_type = desc.motion_type;
        handle.generation = impl_->bodies[handle.index].generation;
    }
    else
    {
        handle = {static_cast<std::uint32_t>(impl_->bodies.size()), 1};
        impl_->bodies.push_back({body_id, handle.generation, desc.motion_type});
    }

    ++impl_->body_count;
    impl_->body_handles.emplace(body_id.GetIndexAndSequenceNumber(), handle);
    return handle;
}

void PhysicsSystem::DestroyBody(PhysicsBodyHandle body)
{
    if (impl_ == nullptr || !impl_->initialized || impl_->physics_system == nullptr)
    {
        return;
    }

    const JPH::BodyID body_id = impl_->GetBodyID(body);
    if (body_id.IsInvalid())
    {
        return;
    }

    for (std::uint32_t index = 0; index < impl_->constraints.size(); ++index)
    {
        const Impl::ConstraintSlot &slot = impl_->constraints[index];
        if (slot.constraint != nullptr && (slot.first == body || slot.second == body))
        {
            DestroyConstraint({index, slot.generation});
        }
    }

    JPH::BodyInterface &body_interface = impl_->physics_system->GetBodyInterface();
    impl_->body_handles.erase(body_id.GetIndexAndSequenceNumber());
    body_interface.RemoveBody(body_id);
    body_interface.DestroyBody(body_id);
    Impl::BodySlot &slot = impl_->bodies[body.index];
    slot.body = JPH::BodyID();
    slot.motion_type = PhysicsMotionType::Static;
    ++slot.generation;
    if (slot.generation == 0)
    {
        ++slot.generation;
    }
    impl_->free_handles.push_back(body.index);
    --impl_->body_count;
}

bool PhysicsSystem::IsValid(PhysicsBodyHandle body) const
{
    return impl_ != nullptr && impl_->initialized && !impl_->GetBodyID(body).IsInvalid();
}

bool PhysicsSystem::GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const
{
    state = {};
    if (impl_ == nullptr || !impl_->initialized || impl_->physics_system == nullptr)
    {
        return false;
    }

    const JPH::BodyID body_id = impl_->GetBodyID(body);
    if (body_id.IsInvalid())
    {
        return false;
    }

    JPH::RVec3 position;
    JPH::Quat rotation{};
    const JPH::BodyInterface &bodies = impl_->physics_system->GetBodyInterface();
    bodies.GetPositionAndRotation(body_id, position, rotation);
    state.position = ToGlmRVec3(position);
    state.rotation = ToGlmQuat(rotation);
    state.linear_velocity = ToGlmVec3(bodies.GetLinearVelocity(body_id));
    state.angular_velocity = ToGlmVec3(bodies.GetAngularVelocity(body_id));
    state.active = bodies.IsActive(body_id);
    return true;
}

bool PhysicsSystem::SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                     bool activate)
{
    if (!Finite(position) || !Finite(rotation) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().SetPositionAndRotation(
        impl_->GetBodyID(body), ToJoltRVec3(position), ToJoltQuat(glm::normalize(rotation)),
        activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    return true;
}

bool PhysicsSystem::MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                  float fixed_delta_time)
{
    if (!Finite(position) || !Finite(rotation) || !std::isfinite(fixed_delta_time) || fixed_delta_time <= 0.0f ||
        !IsValid(body) || impl_->bodies[body.index].motion_type != PhysicsMotionType::Kinematic)
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().MoveKinematic(impl_->GetBodyID(body), ToJoltRVec3(position),
                                                            ToJoltQuat(glm::normalize(rotation)), fixed_delta_time);
    return true;
}

bool PhysicsSystem::SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular)
{
    if (!Finite(linear) || !Finite(angular) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().SetLinearAndAngularVelocity(impl_->GetBodyID(body), ToJoltVec3(linear),
                                                                          ToJoltVec3(angular));
    return true;
}

bool PhysicsSystem::AddForce(PhysicsBodyHandle body, const glm::vec3 &force)
{
    if (!Finite(force) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().AddForce(impl_->GetBodyID(body), ToJoltVec3(force));
    return true;
}

bool PhysicsSystem::AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position)
{
    if (!Finite(force) || !Finite(world_position) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().AddForce(impl_->GetBodyID(body), ToJoltVec3(force),
                                                       ToJoltRVec3(world_position));
    return true;
}

bool PhysicsSystem::AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque)
{
    if (!Finite(torque) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().AddTorque(impl_->GetBodyID(body), ToJoltVec3(torque));
    return true;
}

bool PhysicsSystem::AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse)
{
    if (!Finite(impulse) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().AddImpulse(impl_->GetBodyID(body), ToJoltVec3(impulse));
    return true;
}

bool PhysicsSystem::AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position)
{
    if (!Finite(impulse) || !Finite(world_position) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().AddImpulse(impl_->GetBodyID(body), ToJoltVec3(impulse),
                                                         ToJoltRVec3(world_position));
    return true;
}

bool PhysicsSystem::SetGravityFactor(PhysicsBodyHandle body, float factor)
{
    if (!std::isfinite(factor) || !IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().SetGravityFactor(impl_->GetBodyID(body), factor);
    return true;
}

bool PhysicsSystem::SetSensor(PhysicsBodyHandle body, bool sensor)
{
    if (!IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().SetIsSensor(impl_->GetBodyID(body), sensor);
    return true;
}

bool PhysicsSystem::Activate(PhysicsBodyHandle body)
{
    if (!IsValid(body))
    {
        return false;
    }
    impl_->physics_system->GetBodyInterface().ActivateBody(impl_->GetBodyID(body));
    return true;
}

bool PhysicsSystem::SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled)
{
    if (impl_ == nullptr || !impl_->initialized || first >= Layers::Count || second >= Layers::Count)
    {
        return false;
    }
    const std::uint32_t first_bit = 1u << first;
    const std::uint32_t second_bit = 1u << second;
    if (enabled)
    {
        impl_->object_layer_pair_filter->masks[first] |= second_bit;
        impl_->object_layer_pair_filter->masks[second] |= first_bit;
    }
    else
    {
        impl_->object_layer_pair_filter->masks[first] &= ~second_bit;
        impl_->object_layer_pair_filter->masks[second] &= ~first_bit;
    }
    return true;
}

bool PhysicsSystem::RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                            std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    hit = {};
    if (impl_ == nullptr || !impl_->initialized || !Finite(origin) || !Finite(displacement) ||
        glm::dot(displacement, displacement) <= 0.0f || layer_mask == 0)
    {
        return false;
    }
    JPH::RayCastResult result;
    const QueryLayerFilter layer_filter(layer_mask);
    const QueryBodyFilter body_filter(impl_->GetBodyID(ignore));
    if (!impl_->physics_system->GetNarrowPhaseQuery().CastRay(
            JPH::RRayCast(ToJoltRVec3(origin), ToJoltVec3(displacement)), result, {}, layer_filter, body_filter))
    {
        return false;
    }
    hit.body = impl_->GetBodyHandle(result.mBodyID);
    if (!hit.body)
    {
        return false;
    }
    hit.fraction = result.mFraction;
    hit.position = origin + displacement * result.mFraction;
    hit.sub_shape = result.mSubShapeID2.GetValue();
    const JPH::BodyLockRead lock(impl_->physics_system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded())
    {
        hit.normal =
            ToGlmVec3(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToJoltRVec3(hit.position)));
    }
    return true;
}

std::size_t PhysicsSystem::RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                                      std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    if (hits == nullptr || capacity == 0 || impl_ == nullptr || !impl_->initialized || !Finite(origin) ||
        !Finite(displacement) || glm::dot(displacement, displacement) <= 0.0f || layer_mask == 0)
    {
        return 0;
    }
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    const QueryLayerFilter layer_filter(layer_mask);
    const QueryBodyFilter body_filter(impl_->GetBodyID(ignore));
    impl_->physics_system->GetNarrowPhaseQuery().CastRay(JPH::RRayCast(ToJoltRVec3(origin), ToJoltVec3(displacement)),
                                                         {}, collector, {}, layer_filter, body_filter);
    collector.Sort();
    const std::size_t count = std::min(capacity, collector.mHits.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const JPH::RayCastResult &result = collector.mHits[index];
        PhysicsQueryHit &output = hits[index];
        output = {};
        output.body = impl_->GetBodyHandle(result.mBodyID);
        output.fraction = result.mFraction;
        output.position = origin + displacement * result.mFraction;
        output.sub_shape = result.mSubShapeID2.GetValue();
        const JPH::BodyLockRead lock(impl_->physics_system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded())
        {
            output.normal =
                ToGlmVec3(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToJoltRVec3(output.position)));
        }
    }
    return count;
}

bool PhysicsSystem::ShapeCast(const PhysicsShape &source_shape, const glm::vec3 &position, const glm::quat &rotation,
                              const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                              PhysicsBodyHandle ignore) const
{
    hit = {};
    if (impl_ == nullptr || !impl_->initialized || !ValidateShape(source_shape) ||
        source_shape.type == PhysicsShapeType::TriangleMesh || source_shape.type == PhysicsShapeType::HeightField ||
        !Finite(position) || !Finite(rotation) || !Finite(displacement) ||
        glm::dot(displacement, displacement) <= 0.0f || layer_mask == 0)
    {
        return false;
    }
    const JPH::RefConst<JPH::Shape> shape = BuildShape(source_shape);
    if (shape == nullptr)
    {
        return false;
    }
    const JPH::RMat44 world =
        JPH::RMat44::sRotationTranslation(ToJoltQuat(glm::normalize(rotation)), ToJoltRVec3(position));
    const JPH::RShapeCast cast =
        JPH::RShapeCast::sFromWorldTransform(shape, JPH::Vec3::sOne(), world, ToJoltVec3(displacement));
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const QueryLayerFilter layer_filter(layer_mask);
    const QueryBodyFilter body_filter(impl_->GetBodyID(ignore));
    impl_->physics_system->GetNarrowPhaseQuery().CastShape(cast, {}, JPH::RVec3::sZero(), collector, {}, layer_filter,
                                                           body_filter);
    if (!collector.HadHit())
    {
        return false;
    }
    const JPH::ShapeCastResult &result = collector.mHit;
    hit.body = impl_->GetBodyHandle(result.mBodyID2);
    if (!hit.body)
    {
        return false;
    }
    hit.position = ToGlmVec3(result.mContactPointOn2);
    hit.normal = ToGlmVec3(-result.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisZ()));
    hit.fraction = result.mFraction;
    hit.sub_shape = result.mSubShapeID2.GetValue();
    return true;
}

std::size_t PhysicsSystem::Overlap(const PhysicsShape &source_shape, const glm::vec3 &position,
                                   const glm::quat &rotation, PhysicsBodyHandle *bodies, std::size_t capacity,
                                   std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    if (bodies == nullptr || capacity == 0 || impl_ == nullptr || !impl_->initialized || !ValidateShape(source_shape) ||
        !Finite(position) || !Finite(rotation) || layer_mask == 0)
    {
        return 0;
    }
    const JPH::RefConst<JPH::Shape> shape = BuildShape(source_shape);
    if (shape == nullptr)
    {
        return 0;
    }
    const JPH::RMat44 world =
        JPH::RMat44::sRotationTranslation(ToJoltQuat(glm::normalize(rotation)), ToJoltRVec3(position));
    const JPH::RMat44 center_of_mass = world.PreTranslated(shape->GetCenterOfMass());
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    const QueryLayerFilter layer_filter(layer_mask);
    const QueryBodyFilter body_filter(impl_->GetBodyID(ignore));
    impl_->physics_system->GetNarrowPhaseQuery().CollideShape(
        shape, JPH::Vec3::sOne(), center_of_mass, {}, JPH::RVec3::sZero(), collector, {}, layer_filter, body_filter);
    std::vector<PhysicsBodyHandle> unique;
    unique.reserve(std::min(capacity, collector.mHits.size()));
    for (const JPH::CollideShapeResult &result : collector.mHits)
    {
        const PhysicsBodyHandle handle = impl_->GetBodyHandle(result.mBodyID2);
        if (!handle || handle == ignore || std::find(unique.begin(), unique.end(), handle) != unique.end())
            continue;
        unique.push_back(handle);
    }
    std::sort(unique.begin(), unique.end(), [](PhysicsBodyHandle first, PhysicsBodyHandle second) {
        return first.index < second.index || (first.index == second.index && first.generation < second.generation);
    });
    const std::size_t count = std::min(capacity, unique.size());
    std::copy_n(unique.begin(), count, bodies);
    return count;
}

std::size_t PhysicsSystem::DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity)
{
    if (events == nullptr || capacity == 0 || impl_ == nullptr)
    {
        return 0;
    }
    std::stable_sort(impl_->contact_events.begin(), impl_->contact_events.end(),
                     [](const RawContact &first, const RawContact &second) {
                         if (first.first != second.first)
                         {
                             return first.first < second.first;
                         }
                         if (first.second != second.second)
                         {
                             return first.second < second.second;
                         }
                         return first.type < second.type;
                     });
    std::size_t count = 0;
    std::size_t consumed = 0;
    for (; consumed < impl_->contact_events.size() && count < capacity; ++consumed)
    {
        const RawContact &source = impl_->contact_events[consumed];
        const PhysicsBodyHandle first = impl_->GetBodyHandle(source.first);
        const PhysicsBodyHandle second = impl_->GetBodyHandle(source.second);
        if (!first || !second)
        {
            continue;
        }
        events[count++] = {source.type, first, second, source.position, source.normal, source.sensor};
    }
    impl_->contact_events.erase(impl_->contact_events.begin(),
                                impl_->contact_events.begin() + static_cast<std::ptrdiff_t>(consumed));
    return count;
}

std::size_t PhysicsSystem::PendingContactEventCount() const
{
    return impl_ != nullptr ? impl_->contact_events.size() : 0;
}

std::uint64_t PhysicsSystem::DroppedContactEventCount() const
{
    return impl_ != nullptr ? impl_->dropped_contact_events : 0;
}

PhysicsConstraintHandle PhysicsSystem::CreateConstraint(const PhysicsConstraintDesc &desc)
{
    if (impl_ == nullptr || !impl_->initialized || !IsValid(desc.first) || !IsValid(desc.second) ||
        desc.first == desc.second || !Finite(desc.first_anchor) || !Finite(desc.second_anchor) || !Finite(desc.axis) ||
        !Finite(desc.normal) || !std::isfinite(desc.minimum) || !std::isfinite(desc.maximum) ||
        !std::isfinite(desc.spring_frequency) || !std::isfinite(desc.spring_damping) ||
        !std::isfinite(desc.motor_target) || !std::isfinite(desc.motor_force_limit) ||
        !std::isfinite(desc.motor_frequency) || !std::isfinite(desc.motor_damping) || desc.spring_frequency < 0.0f ||
        desc.spring_damping < 0.0f || desc.motor_force_limit < 0.0f || desc.motor_frequency < 0.0f ||
        desc.motor_damping < 0.0f)
    {
        return {};
    }
    glm::vec3 axis = desc.axis;
    const float axis_length = glm::length(axis);
    if (axis_length <= 1.0e-6f)
    {
        return {};
    }
    axis /= axis_length;
    glm::vec3 normal = desc.normal - (axis * glm::dot(desc.normal, axis));
    const float normal_length = glm::length(normal);
    if (normal_length <= 1.0e-6f)
    {
        return {};
    }
    normal /= normal_length;

    const JPH::BodyID body_ids[] = {impl_->GetBodyID(desc.first), impl_->GetBodyID(desc.second)};
    JPH::BodyLockMultiWrite lock(impl_->physics_system->GetBodyLockInterface(), body_ids, 2);
    JPH::Body *first = lock.GetBody(0);
    JPH::Body *second = lock.GetBody(1);
    if (first == nullptr || second == nullptr)
    {
        return {};
    }

    JPH::Ref<JPH::Constraint> constraint;
    switch (desc.type)
    {
    case PhysicsConstraintType::Fixed: {
        JPH::FixedConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mAutoDetectPoint = true;
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    case PhysicsConstraintType::Point: {
        JPH::PointConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = ToJoltRVec3(desc.first_anchor);
        settings.mPoint2 = ToJoltRVec3(desc.second_anchor);
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    case PhysicsConstraintType::Hinge: {
        if (desc.minimum > 0.0f || desc.maximum < 0.0f || desc.minimum > desc.maximum || desc.minimum < -JPH::JPH_PI ||
            desc.maximum > JPH::JPH_PI)
        {
            return {};
        }
        JPH::HingeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = ToJoltRVec3(desc.first_anchor);
        settings.mPoint2 = ToJoltRVec3(desc.second_anchor);
        settings.mHingeAxis1 = settings.mHingeAxis2 = ToJoltVec3(axis);
        settings.mNormalAxis1 = settings.mNormalAxis2 = ToJoltVec3(normal);
        settings.mLimitsMin = desc.minimum;
        settings.mLimitsMax = desc.maximum;
        settings.mLimitsSpringSettings.mFrequency = desc.spring_frequency;
        settings.mLimitsSpringSettings.mDamping = desc.spring_damping;
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    case PhysicsConstraintType::Slider: {
        if (desc.minimum > desc.maximum)
        {
            return {};
        }
        JPH::SliderConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = ToJoltRVec3(desc.first_anchor);
        settings.mPoint2 = ToJoltRVec3(desc.second_anchor);
        settings.mSliderAxis1 = settings.mSliderAxis2 = ToJoltVec3(axis);
        settings.mNormalAxis1 = settings.mNormalAxis2 = ToJoltVec3(normal);
        settings.mLimitsMin = desc.minimum;
        settings.mLimitsMax = desc.maximum;
        settings.mLimitsSpringSettings.mFrequency = desc.spring_frequency;
        settings.mLimitsSpringSettings.mDamping = desc.spring_damping;
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    case PhysicsConstraintType::Distance: {
        if (desc.minimum < 0.0f || desc.minimum > desc.maximum)
        {
            return {};
        }
        JPH::DistanceConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = ToJoltRVec3(desc.first_anchor);
        settings.mPoint2 = ToJoltRVec3(desc.second_anchor);
        settings.mMinDistance = desc.minimum;
        settings.mMaxDistance = desc.maximum;
        settings.mLimitsSpringSettings.mFrequency = desc.spring_frequency;
        settings.mLimitsSpringSettings.mDamping = desc.spring_damping;
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    case PhysicsConstraintType::Cone: {
        if (desc.maximum < 0.0f || desc.maximum > JPH::JPH_PI)
        {
            return {};
        }
        JPH::ConeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = ToJoltRVec3(desc.first_anchor);
        settings.mPoint2 = ToJoltRVec3(desc.second_anchor);
        settings.mTwistAxis1 = settings.mTwistAxis2 = ToJoltVec3(axis);
        settings.mHalfConeAngle = desc.maximum;
        settings.mEnabled = desc.enabled;
        constraint = settings.Create(*first, *second);
        break;
    }
    }
    if (constraint == nullptr)
    {
        return {};
    }
    impl_->physics_system->AddConstraint(constraint);

    PhysicsConstraintHandle handle;
    if (!impl_->free_constraint_handles.empty())
    {
        handle.index = impl_->free_constraint_handles.back();
        impl_->free_constraint_handles.pop_back();
        Impl::ConstraintSlot &slot = impl_->constraints[handle.index];
        slot.constraint = constraint;
        slot.first = desc.first;
        slot.second = desc.second;
        handle.generation = slot.generation;
    }
    else
    {
        handle = {static_cast<std::uint32_t>(impl_->constraints.size()), 1};
        impl_->constraints.push_back({constraint, desc.first, desc.second, handle.generation});
    }
    ++impl_->constraint_count;
    if (desc.motor != PhysicsMotorMode::Off &&
        !SetConstraintMotor(handle, desc.motor, desc.motor_target, desc.motor_force_limit, desc.motor_frequency,
                            desc.motor_damping))
    {
        DestroyConstraint(handle);
        return {};
    }
    return handle;
}

void PhysicsSystem::DestroyConstraint(PhysicsConstraintHandle constraint)
{
    if (impl_ == nullptr || !impl_->initialized)
    {
        return;
    }
    JPH::Constraint *value = impl_->GetConstraint(constraint);
    if (value == nullptr)
    {
        return;
    }
    impl_->physics_system->RemoveConstraint(value);
    Impl::ConstraintSlot &slot = impl_->constraints[constraint.index];
    slot.constraint = nullptr;
    slot.first = {};
    slot.second = {};
    ++slot.generation;
    if (slot.generation == 0)
    {
        ++slot.generation;
    }
    impl_->free_constraint_handles.push_back(constraint.index);
    --impl_->constraint_count;
}

bool PhysicsSystem::SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled)
{
    JPH::Constraint *value = impl_ != nullptr ? impl_->GetConstraint(constraint) : nullptr;
    if (value == nullptr)
    {
        return false;
    }
    value->SetEnabled(enabled);
    return true;
}

bool PhysicsSystem::SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target,
                                       float force_limit, float frequency, float damping)
{
    JPH::Constraint *value = impl_ != nullptr ? impl_->GetConstraint(constraint) : nullptr;
    if (value == nullptr || !std::isfinite(target) || !std::isfinite(force_limit) || !std::isfinite(frequency) ||
        !std::isfinite(damping) || force_limit < 0.0f || frequency < 0.0f || damping < 0.0f ||
        (mode != PhysicsMotorMode::Off && force_limit <= 0.0f))
    {
        return false;
    }
    const JPH::EMotorState state = mode == PhysicsMotorMode::Velocity   ? JPH::EMotorState::Velocity
                                   : mode == PhysicsMotorMode::Position ? JPH::EMotorState::Position
                                                                        : JPH::EMotorState::Off;
    switch (value->GetSubType())
    {
    case JPH::EConstraintSubType::Hinge: {
        auto &hinge = static_cast<JPH::HingeConstraint &>(*value);
        JPH::MotorSettings &settings = hinge.GetMotorSettings();
        settings.mSpringSettings.mFrequency = frequency;
        settings.mSpringSettings.mDamping = damping;
        settings.SetTorqueLimit(force_limit);
        hinge.SetTargetAngularVelocity(target);
        hinge.SetTargetAngle(target);
        hinge.SetMotorState(state);
        return true;
    }
    case JPH::EConstraintSubType::Slider: {
        auto &slider = static_cast<JPH::SliderConstraint &>(*value);
        JPH::MotorSettings &settings = slider.GetMotorSettings();
        settings.mSpringSettings.mFrequency = frequency;
        settings.mSpringSettings.mDamping = damping;
        settings.SetForceLimit(force_limit);
        slider.SetTargetVelocity(target);
        slider.SetTargetPosition(target);
        slider.SetMotorState(state);
        return true;
    }
    default:
        return false;
    }
}

bool PhysicsSystem::IsValid(PhysicsConstraintHandle constraint) const
{
    return impl_ != nullptr && impl_->GetConstraint(constraint) != nullptr;
}

std::size_t PhysicsSystem::ConstraintCount() const
{
    return impl_ != nullptr ? impl_->constraint_count : 0;
}

PhysicsCharacterHandle PhysicsSystem::CreateCharacter(const PhysicsCharacterDesc &desc)
{
    if (impl_ == nullptr || !impl_->initialized || !Finite(desc.position) || !std::isfinite(desc.mass) ||
        desc.mass <= 0.0f || !std::isfinite(desc.max_strength) || desc.max_strength < 0.0f ||
        !std::isfinite(desc.max_slope_radians) || desc.max_slope_radians < 0.0f ||
        desc.max_slope_radians >= 0.5f * JPH::JPH_PI || !std::isfinite(desc.step_height) || desc.step_height < 0.0f ||
        !std::isfinite(desc.stick_to_floor_distance) || desc.stick_to_floor_distance < 0.0f ||
        !std::isfinite(desc.padding) || desc.padding <= 0.0f || !std::isfinite(desc.penetration_recovery) ||
        desc.penetration_recovery < 0.0f || desc.penetration_recovery > 1.0f || desc.collision_layer >= Layers::Count)
    {
        return {};
    }
    const JPH::RefConst<JPH::Shape> shape = BuildCharacterShape(desc.radius, desc.height);
    if (shape == nullptr)
    {
        return {};
    }

    JPH::CharacterVirtualSettings settings;
    settings.mUp = JPH::Vec3::sAxisZ();
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisZ(), -desc.radius);
    settings.mShape = shape;
    settings.mMass = desc.mass;
    settings.mMaxStrength = desc.max_strength;
    settings.mMaxSlopeAngle = desc.max_slope_radians;
    settings.mCharacterPadding = desc.padding;
    settings.mPenetrationRecoverySpeed = desc.penetration_recovery;
    settings.mEnhancedInternalEdgeRemoval = true;
    JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
        &settings, ToJoltRVec3(desc.position), JPH::Quat::sIdentity(), impl_->physics_system.get());

    PhysicsCharacterHandle handle;
    if (!impl_->free_character_handles.empty())
    {
        handle.index = impl_->free_character_handles.back();
        impl_->free_character_handles.pop_back();
        Impl::CharacterSlot &slot = impl_->characters[handle.index];
        slot.character = character;
        slot.radius = desc.radius;
        slot.height = desc.height;
        slot.step_height = desc.step_height;
        slot.stick_to_floor_distance = desc.stick_to_floor_distance;
        slot.collision_layer = desc.collision_layer;
        handle.generation = slot.generation;
    }
    else
    {
        handle = {static_cast<std::uint32_t>(impl_->characters.size()), 1};
        impl_->characters.push_back({character, desc.radius, desc.height, desc.step_height,
                                     desc.stick_to_floor_distance, desc.collision_layer, handle.generation});
    }
    ++impl_->character_count;
    return handle;
}

void PhysicsSystem::DestroyCharacter(PhysicsCharacterHandle character)
{
    if (impl_ == nullptr || !impl_->initialized || impl_->GetCharacter(character) == nullptr)
    {
        return;
    }
    Impl::CharacterSlot &slot = impl_->characters[character.index];
    slot.character = nullptr;
    ++slot.generation;
    if (slot.generation == 0)
    {
        ++slot.generation;
    }
    impl_->free_character_handles.push_back(character.index);
    --impl_->character_count;
}

bool PhysicsSystem::IsValid(PhysicsCharacterHandle character) const
{
    return impl_ != nullptr && impl_->GetCharacter(character) != nullptr;
}

bool PhysicsSystem::SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity)
{
    JPH::CharacterVirtual *value = impl_ != nullptr ? impl_->GetCharacter(character) : nullptr;
    if (value == nullptr || !Finite(velocity))
    {
        return false;
    }
    value->SetLinearVelocity(ToJoltVec3(velocity));
    return true;
}

bool PhysicsSystem::UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time)
{
    JPH::CharacterVirtual *value = impl_ != nullptr ? impl_->GetCharacter(character) : nullptr;
    if (value == nullptr || !std::isfinite(fixed_delta_time) || fixed_delta_time <= 0.0f ||
        impl_->temp_allocator == nullptr)
    {
        return false;
    }
    value->SetLinearVelocity(value->GetLinearVelocity() + (impl_->physics_system->GetGravity() * fixed_delta_time));
    const Impl::CharacterSlot &slot = impl_->characters[character.index];
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mStickToFloorStepDown = JPH::Vec3(0.0f, 0.0f, -slot.stick_to_floor_distance);
    settings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.0f, slot.step_height);
    const JPH::ObjectLayer object_layer = ToJoltLayer(PhysicsMotionType::Kinematic, slot.collision_layer);
    value->ExtendedUpdate(fixed_delta_time, impl_->physics_system->GetGravity(), settings,
                          impl_->physics_system->GetDefaultBroadPhaseLayerFilter(object_layer),
                          impl_->physics_system->GetDefaultLayerFilter(object_layer), {}, {}, *impl_->temp_allocator);
    return true;
}

bool PhysicsSystem::SetCharacterHeight(PhysicsCharacterHandle character, float height)
{
    JPH::CharacterVirtual *value = impl_ != nullptr ? impl_->GetCharacter(character) : nullptr;
    if (value == nullptr || impl_->temp_allocator == nullptr)
    {
        return false;
    }
    Impl::CharacterSlot &slot = impl_->characters[character.index];
    const JPH::RefConst<JPH::Shape> shape = BuildCharacterShape(slot.radius, height);
    if (shape == nullptr)
    {
        return false;
    }
    const JPH::ObjectLayer object_layer = ToJoltLayer(PhysicsMotionType::Kinematic, slot.collision_layer);
    if (!value->SetShape(shape, 0.05f, impl_->physics_system->GetDefaultBroadPhaseLayerFilter(object_layer),
                         impl_->physics_system->GetDefaultLayerFilter(object_layer), {}, {}, *impl_->temp_allocator))
    {
        return false;
    }
    slot.height = height;
    return true;
}

bool PhysicsSystem::GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const
{
    state = {};
    JPH::CharacterVirtual *value = impl_ != nullptr ? impl_->GetCharacter(character) : nullptr;
    if (value == nullptr)
    {
        return false;
    }
    state.position = ToGlmRVec3(value->GetPosition());
    state.velocity = ToGlmVec3(value->GetLinearVelocity());
    state.ground_normal = ToGlmVec3(value->GetGroundNormal());
    state.ground_velocity = ToGlmVec3(value->GetGroundVelocity());
    state.ground_body = impl_->GetBodyHandle(value->GetGroundBodyID());
    state.grounded = value->IsSupported();
    state.slope_too_steep = value->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnSteepGround;
    return true;
}

std::size_t PhysicsSystem::CharacterCount() const
{
    return impl_ != nullptr ? impl_->character_count : 0;
}

std::size_t PhysicsSystem::BodyCount() const
{
    return impl_ != nullptr ? impl_->body_count : 0;
}
