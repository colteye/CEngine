#ifndef CENGINE_ASSETS_SCENE_LOADER_H
#define CENGINE_ASSETS_SCENE_LOADER_H

#include <filesystem>
#include <memory>

namespace CEngine::Entities { class EntityFactory; }
namespace CEngine::Scene { class Scene; }

namespace CEngine::Assets {

class AssetDatabase;

std::unique_ptr<Scene::Scene> LoadScene(
    const std::filesystem::path& path, AssetDatabase& assets);
std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path& path,
    AssetDatabase& assets, Entities::EntityFactory& entity_factory);

} // namespace CEngine::Assets

#endif
