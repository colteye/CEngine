#ifndef CENGINE_ASSETS_PHYSICS_LOADER_H
#define CENGINE_ASSETS_PHYSICS_LOADER_H

#include "physics/physics_types.h"

#include <filesystem>

namespace CEngine::Assets
{

bool LoadPhysicsAsset(const std::filesystem::path &path, PhysicsShape &shape);

} // namespace CEngine::Assets

#endif
