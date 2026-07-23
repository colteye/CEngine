//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/scene_loader.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_SCENE_LOADER_H
#define CENGINE_ASSETS_SCENE_LOADER_H

#include <filesystem>
#include <memory>

namespace CEngine::Entities
{
class EntityFactory;
}
namespace CEngine::Scene
{
class Scene;
}

namespace CEngine::Assets
{

class AssetStore;

/**
 * @brief TODO: Describe LoadScene.
 *
 * @param path TODO: Describe this parameter.
 * @param store TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path &path, AssetStore &store);
/**
 * @brief TODO: Describe LoadScene.
 *
 * @param path TODO: Describe this parameter.
 * @param store TODO: Describe this parameter.
 * @param entity_factory TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path &path, AssetStore &store,
                                        Entities::EntityFactory &entity_factory);

} // namespace CEngine::Assets

#endif
