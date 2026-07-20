#ifndef JOLT_PHYSICS_BACKEND_H
#define JOLT_PHYSICS_BACKEND_H

#include "physics/physics_backend.h"

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)

#include <memory>

class JoltPhysicsBackend final : public IPhysicsBackend
{
public:
	JoltPhysicsBackend();
	~JoltPhysicsBackend() override;

	bool Initialize(const PhysicsSystemDesc& desc) override;
	void Shutdown() override;
	void Step(float fixed_delta_time) override;

	PhysicsBodyHandle CreateBody(const PhysicsBodyDesc& desc) override;
	void DestroyBody(PhysicsBodyHandle body) override;
	glm::mat4 GetBodyTransform(PhysicsBodyHandle body) const override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

#endif

#endif
