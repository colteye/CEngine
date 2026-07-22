#include "entity/entity_factory.h"

#include "entity/camera_entity.h"
#include "entity/empty_entity.h"
#include "entity/exponential_height_fog_entity.h"
#include "entity/light_entity.h"
#include "entity/player_start.h"
#include "entity/prefab_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
#include "entity/trigger_entity.h"

namespace CEngine::Entities {

std::unique_ptr<Scene::Entity> MakeEntity(std::string_view classname)
{
    if (classname == "empty") return std::make_unique<EmptyEntity>();
    if (classname == "prop") return std::make_unique<PropEntity>();
    if (classname == "light") return std::make_unique<LightEntity>();
    if (classname == "camera") return std::make_unique<CameraEntity>();
    if (classname == "trigger") return std::make_unique<TriggerEntity>();
    if (classname == "prefab_instance") return std::make_unique<PrefabEntity>();
    if (classname == "info_player_start") return std::make_unique<PlayerStart>();
    if (classname == "skybox") return std::make_unique<SkyboxEntity>();
    if (classname == "exponential_height_fog") return std::make_unique<ExponentialHeightFogEntity>();
    return nullptr;
}

} // namespace CEngine::Entities
