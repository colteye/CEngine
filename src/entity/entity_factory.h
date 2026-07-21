#ifndef CENGINE_ENTITY_ENTITY_FACTORY_H
#define CENGINE_ENTITY_ENTITY_FACTORY_H

#include <memory>
#include <string_view>

namespace CEngine::Scene { class Entity; }

namespace CEngine::Entities {

std::unique_ptr<Scene::Entity> MakeEntity(std::string_view classname);

} // namespace CEngine::Entities

#endif
