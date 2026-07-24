//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/audio_entities.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_AUDIO_ENTITIES_H
#define CENGINE_ENTITY_AUDIO_ENTITIES_H

#include "audio/audio_system.h"
#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

#include <memory>

namespace CEngine::Assets
{
using Audio = std::vector<std::byte>;
}

namespace CEngine::Entities
{

class AudioSourceEntity final : public Scene::Entity, public Generated::EngineEntities::AudioSourceProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;

  protected:
    void OnEnabledChanged(Context &context, bool enabled) override;

  private:
    bool Play(Context &context, Scene::EntityHandle activator);
    bool Stop(Context &context, Scene::EntityHandle activator, bool emit);
    [[nodiscard]] Audio::SpatialDesc Spatial() const;

    std::shared_ptr<const Assets::Audio> loaded_audio_;
    Audio::VoiceHandle voice_;
    glm::vec3 previous_position_ = glm::vec3(0.0f);
    bool position_valid_ = false;
    bool was_playing_ = false;
};

class AudioEnvironmentEntity final : public Scene::Entity,
                                     public Generated::EngineEntities::AudioEnvironmentProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;

  protected:
    void OnEnabledChanged(Context &context, bool enabled) override;
};

} // namespace CEngine::Entities

#endif
