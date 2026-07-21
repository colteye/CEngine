#ifndef CENGINE_SCENE_SCENE_RENDER_STATE_H
#define CENGINE_SCENE_SCENE_RENDER_STATE_H

#include "assets/asset_handle.h"
#include "entity/entity.h"
#include "renderer/light.h"
#include "renderer/lightmap.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/renderable.h"

#include <string>
#include <vector>

namespace CEngine::Assets { class AssetDatabase; }

namespace CEngine::Scene {

class Scene;

class SceneRenderState {
public:
    ~SceneRenderState();

    bool Activate(const Scene& scene, Assets::AssetDatabase& assets, std::string* error = nullptr);
    void Stop();
    void UpdateDynamic(const Scene& scene);
    bool Active() const { return active_; }

private:
    struct LoadedMaterial {
        Assets::AssetHandle asset;
        Renderer::Material material;
    };

    struct LoadedMesh {
        Assets::AssetHandle asset;
        Assets::AssetHandle material_asset;
        Renderer::Mesh mesh;
    };

    struct LoadedLightmap {
        Assets::AssetHandle asset;
        Renderer::Lightmap lightmap;
    };

    struct DynamicRenderable {
        EntityId entity;
        Renderer::RenderableHandle renderable;
    };

    Renderer::Material* FindMaterial(Assets::AssetHandle asset);
    Renderer::Mesh* FindMesh(Assets::AssetHandle asset, Assets::AssetHandle material);
    Renderer::Lightmap* FindLightmap(Assets::AssetHandle asset);

    std::vector<LoadedMaterial> materials_;
    std::vector<LoadedMesh> meshes_;
    std::vector<LoadedLightmap> lightmaps_;
    std::vector<Renderer::RenderableHandle> renderables_;
    std::vector<Renderer::LightHandle> lights_;
    std::vector<DynamicRenderable> dynamic_renderables_;
    bool active_ = false;
};

} // namespace CEngine::Scene

#endif
