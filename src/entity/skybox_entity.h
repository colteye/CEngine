#ifndef CENGINE_ENTITY_SKYBOX_ENTITY_H
#define CENGINE_ENTITY_SKYBOX_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "renderer/texture.h"

#include <memory>
#include <string_view>

namespace CEngine::Entities
{

class SkyboxEntity final : public Scene::Entity, public Generated::EngineEntities::SkyboxProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;

  private:
    std::shared_ptr<const Renderer::Texture> panorama_;
};

} // namespace CEngine::Entities

#endif
