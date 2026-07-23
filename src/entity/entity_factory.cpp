//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/entity_factory.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/entity_factory.h"

#include "engine/engine_entities.generated.h"
#include "entity/fog_entity.h"
#include "entity/light_entity.h"
#include "entity/physics_constraint_entity.h"
#include "entity/post_process_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
#include "logging/logger.h"

namespace CEngine::Entities
{

/**
 * @brief TODO: Describe EntityFactory::EntityFactory.
 */
EntityFactory::EntityFactory()
{
    Register<PropEntity, Generated::EngineEntities::Prop>("prop");
    Register<PhysicsConstraintEntity, Generated::EngineEntities::PhysicsConstraint>("physics_constraint");
    Register<LightEntity, Generated::EngineEntities::Light>("light");
    Register<SkyboxEntity, Generated::EngineEntities::Skybox>("skybox");
    Register<FogEntity, Generated::EngineEntities::ExponentialHeightFog>("exponential_height_fog");
    Register<PostProcessEntity, Generated::EngineEntities::PostProcess>("post_process");
}

/**
 * @brief TODO: Describe EntityFactory::Register.
 *
 * @param classname TODO: Describe this parameter.
 * @param version TODO: Describe this parameter.
 * @param loader TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool EntityFactory::Register(std::string classname, std::uint16_t version, Loader loader)
{
    if (classname.empty() || version == 0 || !loader)
    {
        return false;
    }
    return registrations_.emplace(std::move(classname), Registration{version, std::move(loader)}).second;
}

/**
 * @brief TODO: Describe EntityFactory::Load.
 *
 * @param classname TODO: Describe this parameter.
 * @param version TODO: Describe this parameter.
 * @param bytes TODO: Describe this parameter.
 * @param size TODO: Describe this parameter.
 * @param auxiliary_base TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::unique_ptr<Scene::Entity> EntityFactory::Load(std::string_view classname, std::uint16_t version,
                                                   const std::uint8_t *bytes, std::size_t size,
                                                   std::uint32_t auxiliary_base) const
{
    const auto found = registrations_.find(std::string(classname));
    if (found == registrations_.end())
    {
        Logging::Logger::Get().Error("scene", "entity type is not registered: " + std::string(classname));
        return nullptr;
    }
    if (found->second.version != version)
    {
        Logging::Logger::Get().Error("scene", "entity schema version is unsupported: " + std::string(classname));
        return nullptr;
    }
    std::unique_ptr<Scene::Entity> entity = found->second.loader(bytes, size, auxiliary_base);
    if (entity == nullptr)
    {
        Logging::Logger::Get().Error("scene", "entity data is invalid: " + std::string(classname));
    }
    return entity;
}

} // namespace CEngine::Entities
