//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#ifndef CENGINE_ENTITY_ENVIRONMENT_PROBE_ENTITY_H
#define CENGINE_ENTITY_ENVIRONMENT_PROBE_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "renderer/environment_probe.h"

#include <memory>
#include <string_view>

namespace CEngine::Entities
{

class EnvironmentProbeEntity final : public Scene::Entity,
                                     public Generated::EngineEntities::EnvironmentProbeProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;

  private:
    [[nodiscard]] Renderer::EnvironmentProbe RenderProbe() const;
    void OnEnabledChanged(Context &context, bool enabled) override;

    std::shared_ptr<const Renderer::Texture> panorama_;
    Renderer::EnvironmentProbeHandle renderer_probe_;
};

} // namespace CEngine::Entities

#endif
