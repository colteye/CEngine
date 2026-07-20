#include "physics/jolt/jolt_physics_backend.h"

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
namespace Layers {
constexpr JPH::ObjectLayer NonMoving = 0;
constexpr JPH::ObjectLayer Moving = 1;
constexpr JPH::ObjectLayer NumLayers = 2;
}

namespace BroadPhaseLayers {
const JPH::BroadPhaseLayer NonMoving(0);
const JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint NumLayers = 2;
}

JPH::Vec3 ToJoltVec3(const glm::vec3& value)
{
	return JPH::Vec3(value.x, value.y, value.z);
}

JPH::RVec3 ToJoltRVec3(const glm::vec3& value)
{
	return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Quat ToJoltQuat(const glm::quat& value)
{
	return JPH::Quat(value.x, value.y, value.z, value.w);
}

glm::vec3 ToGlmRVec3(const JPH::RVec3& value)
{
	return glm::vec3(
		static_cast<float>(value.GetX()),
		static_cast<float>(value.GetY()),
		static_cast<float>(value.GetZ()));
}

glm::quat ToGlmQuat(const JPH::Quat& value)
{
	return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
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
	return motion_type == PhysicsMotionType::Static ? Layers::NonMoving : Layers::Moving;
}
} // namespace

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override
	{
		switch (object1)
		{
		case Layers::NonMoving:
			return object2 == Layers::Moving;
		case Layers::Moving:
			return true;
		default:
			return false;
		}
	}
};

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BroadPhaseLayerInterfaceImpl()
	{
		object_to_broad_phase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
		object_to_broad_phase[Layers::Moving] = BroadPhaseLayers::Moving;
	}

	JPH::uint GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::NumLayers;
	}

	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
	{
		if (layer >= Layers::NumLayers)
		{
			return BroadPhaseLayers::NonMoving;
		}

		return object_to_broad_phase[layer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
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
	JPH::BroadPhaseLayer object_to_broad_phase[Layers::NumLayers];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
	{
		switch (layer1)
		{
		case Layers::NonMoving:
			return layer2 == BroadPhaseLayers::Moving;
		case Layers::Moving:
			return true;
		default:
			return false;
		}
	}
};

struct JoltPhysicsBackend::Impl
{
	JPH::BodyID GetBodyID(PhysicsBodyHandle handle) const
	{
		if (handle >= bodies.size())
		{
			return JPH::BodyID();
		}

		return bodies[handle];
	}

	std::unique_ptr<BroadPhaseLayerInterfaceImpl> broad_phase_layer_interface;
	std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> object_vs_broad_phase_layer_filter;
	std::unique_ptr<ObjectLayerPairFilterImpl> object_layer_pair_filter;
	std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
	std::unique_ptr<JPH::JobSystemThreadPool> job_system;
	std::unique_ptr<JPH::PhysicsSystem> physics_system;
	std::vector<JPH::BodyID> bodies;
	std::vector<PhysicsBodyHandle> free_handles;
	bool initialized = false;
	bool owns_jolt_runtime = false;
};

JoltPhysicsBackend::JoltPhysicsBackend() = default;

JoltPhysicsBackend::~JoltPhysicsBackend()
{
	Shutdown();
}

bool JoltPhysicsBackend::Initialize(const PhysicsSystemDesc& desc)
{
	Shutdown();
	impl = std::make_unique<Impl>();

	if (JPH::Factory::sInstance == nullptr)
	{
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
		impl->owns_jolt_runtime = true;
	}

	impl->broad_phase_layer_interface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
	impl->object_vs_broad_phase_layer_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
	impl->object_layer_pair_filter = std::make_unique<ObjectLayerPairFilterImpl>();
	impl->temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

	const uint32_t hardware_threads = std::max(1u, std::thread::hardware_concurrency());
	const uint32_t worker_threads = hardware_threads > 1u ? hardware_threads - 1u : 1u;
	impl->job_system = std::make_unique<JPH::JobSystemThreadPool>(
		JPH::cMaxPhysicsJobs,
		JPH::cMaxPhysicsBarriers,
		static_cast<int>(worker_threads));

	impl->physics_system = std::make_unique<JPH::PhysicsSystem>();
	impl->physics_system->Init(
		desc.max_bodies,
		0,
		desc.max_body_pairs,
		desc.max_contact_constraints,
		*impl->broad_phase_layer_interface,
		*impl->object_vs_broad_phase_layer_filter,
		*impl->object_layer_pair_filter);
	impl->physics_system->SetGravity(ToJoltVec3(desc.gravity));

	impl->initialized = true;
	return true;
}

void JoltPhysicsBackend::Shutdown()
{
	if (impl == nullptr)
	{
		return;
	}

	if (impl->physics_system != nullptr)
	{
		JPH::BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
		for (JPH::BodyID body_id : impl->bodies)
		{
			if (!body_id.IsInvalid())
			{
				body_interface.RemoveBody(body_id);
				body_interface.DestroyBody(body_id);
			}
		}
	}

	impl->bodies.clear();
	impl->free_handles.clear();
	impl->physics_system.reset();
	impl->job_system.reset();
	impl->temp_allocator.reset();
	impl->object_layer_pair_filter.reset();
	impl->object_vs_broad_phase_layer_filter.reset();
	impl->broad_phase_layer_interface.reset();
	impl->initialized = false;

	if (impl->owns_jolt_runtime)
	{
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	impl.reset();
}

void JoltPhysicsBackend::Step(float fixed_delta_time)
{
	if (impl == nullptr || !impl->initialized || impl->physics_system == nullptr ||
		impl->temp_allocator == nullptr || impl->job_system == nullptr)
	{
		return;
	}

	impl->physics_system->Update(fixed_delta_time, 1, impl->temp_allocator.get(), impl->job_system.get());
}

PhysicsBodyHandle JoltPhysicsBackend::CreateBody(const PhysicsBodyDesc& desc)
{
	if (impl == nullptr || !impl->initialized || impl->physics_system == nullptr)
	{
		return kInvalidPhysicsBodyHandle;
	}

	JPH::RefConst<JPH::Shape> shape;
	switch (desc.shape_type)
	{
	case PhysicsShapeType::Box:
		shape = new JPH::BoxShape(ToJoltVec3(desc.box_half_extents));
		break;
	case PhysicsShapeType::Sphere:
		shape = new JPH::SphereShape(desc.sphere_radius);
		break;
	}

	JPH::BodyCreationSettings settings(
		shape,
		ToJoltRVec3(desc.position),
		ToJoltQuat(desc.rotation),
		ToJoltMotionType(desc.motion_type),
		ToJoltLayer(desc.motion_type));
	settings.mFriction = desc.friction;
	settings.mRestitution = desc.restitution;
	if (desc.motion_type == PhysicsMotionType::Dynamic && desc.mass > 0.0f)
	{
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = desc.mass;
	}

	JPH::BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
	JPH::BodyID body_id = body_interface.CreateAndAddBody(
		settings,
		desc.motion_type == PhysicsMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
	if (body_id.IsInvalid())
	{
		std::cout << "Failed to create Jolt body.\n";
		return kInvalidPhysicsBodyHandle;
	}

	if (desc.motion_type != PhysicsMotionType::Static)
	{
		body_interface.SetLinearVelocity(body_id, ToJoltVec3(desc.linear_velocity));
		body_interface.SetAngularVelocity(body_id, ToJoltVec3(desc.angular_velocity));
	}

	PhysicsBodyHandle handle = kInvalidPhysicsBodyHandle;
	if (!impl->free_handles.empty())
	{
		handle = impl->free_handles.back();
		impl->free_handles.pop_back();
		impl->bodies[handle] = body_id;
	}
	else
	{
		handle = static_cast<PhysicsBodyHandle>(impl->bodies.size());
		impl->bodies.push_back(body_id);
	}

	return handle;
}

void JoltPhysicsBackend::DestroyBody(PhysicsBodyHandle body)
{
	if (impl == nullptr || !impl->initialized || impl->physics_system == nullptr)
	{
		return;
	}

	const JPH::BodyID body_id = impl->GetBodyID(body);
	if (body_id.IsInvalid())
	{
		return;
	}

	JPH::BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
	body_interface.RemoveBody(body_id);
	body_interface.DestroyBody(body_id);
	impl->bodies[body] = JPH::BodyID();
	impl->free_handles.push_back(body);
}

glm::mat4 JoltPhysicsBackend::GetBodyTransform(PhysicsBodyHandle body) const
{
	if (impl == nullptr || !impl->initialized || impl->physics_system == nullptr)
	{
		return glm::mat4(1.0f);
	}

	const JPH::BodyID body_id = impl->GetBodyID(body);
	if (body_id.IsInvalid())
	{
		return glm::mat4(1.0f);
	}

	JPH::RVec3 position;
	JPH::Quat rotation;
	impl->physics_system->GetBodyInterface().GetPositionAndRotation(body_id, position, rotation);
	return glm::translate(glm::mat4(1.0f), ToGlmRVec3(position)) * glm::toMat4(ToGlmQuat(rotation));
}

#endif
