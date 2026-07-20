#ifndef PHYSICS_SYSTEM_H
#define PHYSICS_SYSTEM_H

#include "physics/physics_backend.h"
#include "physics/physics_types.h"

#include <memory>

class PhysicsSystem
{
public:
	explicit PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend);
	~PhysicsSystem();

	PhysicsSystem(const PhysicsSystem&) = delete;
	PhysicsSystem& operator=(const PhysicsSystem&) = delete;
	PhysicsSystem(PhysicsSystem&&) noexcept = default;
	PhysicsSystem& operator=(PhysicsSystem&&) noexcept = default;

	bool Initialize(const PhysicsSystemDesc& desc);
	void Shutdown();
	void Step(float delta_time);

	PhysicsBodyHandle CreateBody(const PhysicsBodyDesc& desc);
	void DestroyBody(PhysicsBodyHandle body);
	glm::mat4 GetBodyTransform(PhysicsBodyHandle body) const;

private:
	std::unique_ptr<IPhysicsBackend> backend;
	PhysicsSystemDesc desc;
	float accumulated_time = 0.0f;
	bool initialized = false;
};

#endif
