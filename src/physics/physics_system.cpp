#include "physics/physics_system.h"

#include <algorithm>

PhysicsSystem::PhysicsSystem(std::unique_ptr<IPhysicsBackend> in_backend)
	: backend(std::move(in_backend))
{
}

PhysicsSystem::~PhysicsSystem()
{
	Shutdown();
}

bool PhysicsSystem::Initialize(const PhysicsSystemDesc& in_desc)
{
	Shutdown();
	if (backend == nullptr)
	{
		return false;
	}

	desc = in_desc;
	accumulated_time = 0.0f;
	initialized = backend->Initialize(desc);
	return initialized;
}

void PhysicsSystem::Shutdown()
{
	if (initialized && backend != nullptr)
	{
		backend->Shutdown();
	}

	accumulated_time = 0.0f;
	initialized = false;
}

void PhysicsSystem::Step(float delta_time)
{
	if (!initialized || backend == nullptr || delta_time <= 0.0f || desc.fixed_time_step <= 0.0f)
	{
		return;
	}

	accumulated_time += std::min(delta_time, desc.fixed_time_step * static_cast<float>(desc.max_sub_steps));
	uint32_t steps = 0;
	while (accumulated_time >= desc.fixed_time_step && steps < desc.max_sub_steps)
	{
		backend->Step(desc.fixed_time_step);
		accumulated_time -= desc.fixed_time_step;
		++steps;
	}
}

PhysicsBodyHandle PhysicsSystem::CreateBody(const PhysicsBodyDesc& body_desc)
{
	if (!initialized || backend == nullptr)
	{
		return kInvalidPhysicsBodyHandle;
	}

	return backend->CreateBody(body_desc);
}

void PhysicsSystem::DestroyBody(PhysicsBodyHandle body)
{
	if (initialized && backend != nullptr)
	{
		backend->DestroyBody(body);
	}
}

glm::mat4 PhysicsSystem::GetBodyTransform(PhysicsBodyHandle body) const
{
	if (!initialized || backend == nullptr)
	{
		return glm::mat4(1.0f);
	}

	return backend->GetBodyTransform(body);
}
