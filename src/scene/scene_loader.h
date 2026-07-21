#ifndef CENGINE_SCENE_SCENE_LOADER_H
#define CENGINE_SCENE_SCENE_LOADER_H

#include <filesystem>
#include <memory>
#include <string>

namespace CEngine::Assets { class AssetDatabase; }

namespace CEngine::Scene {

class Scene;

std::unique_ptr<Scene> LoadScene(const std::filesystem::path& path,
    Assets::AssetDatabase& assets, std::string* error = nullptr);

} // namespace CEngine::Scene

#endif
