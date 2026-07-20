#ifndef PHYSICS_BACKEND_H
#define PHYSICS_BACKEND_H

#include "physics/physics_types.h"

#include <glm/glm.hpp>

class IPhysicsBackend
{
public:
	virtual ~IPhysicsBackend() = default;

	virtual bool Initialize(const PhysicsSystemDesc& desc) = 0;
	virtual void Shutdown() = 0;
	virtual void Step(float fixed_delta_time) = 0;

	virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc& desc) = 0;
	virtual void DestroyBody(PhysicsBodyHandle body) = 0;
	virtual glm::mat4 GetBodyTransform(PhysicsBodyHandle body) const = 0;
};

#endif
