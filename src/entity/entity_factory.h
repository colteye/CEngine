#ifndef CENGINE_ENTITY_ENTITY_FACTORY_H
#define CENGINE_ENTITY_ENTITY_FACTORY_H

#include "entity/entity.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace CEngine::Entities
{

class EntityFactory
{
  public:
    using Loader = std::function<std::unique_ptr<Scene::Entity>(const std::uint8_t *, std::size_t, std::uint32_t)>;

    EntityFactory();

    template <typename EntityType, typename SerializedType, typename... Args>
    bool Register(std::string classname, Args &&...args)
    {
        static_assert(std::is_base_of_v<Scene::Entity, EntityType>);
        using Properties = typename SerializedType::Properties;
        static_assert(std::is_base_of_v<Properties, EntityType>);
        auto constructor_args = std::make_tuple(std::forward<Args>(args)...);
        return Register(
            std::move(classname), SerializedType::Version,
            [constructor_args =
                 std::move(constructor_args)](const std::uint8_t *bytes, std::size_t size,
                                              std::uint32_t auxiliary_base) -> std::unique_ptr<Scene::Entity> {
                auto entity = std::apply([](const auto &...values) { return std::make_unique<EntityType>(values...); },
                                         constructor_args);
                SerializedType serialized;
                if (!Read(bytes, size, auxiliary_base, serialized))
                {
                    return nullptr;
                }
                static_cast<Properties &>(*entity) = serialized.properties;
                ApplyTransform(serialized.transform, entity->GetTransform());
                return entity;
            });
    }

    bool Register(std::string classname, std::uint16_t version, Loader loader);
    std::unique_ptr<Scene::Entity> Load(std::string_view classname, std::uint16_t version, const std::uint8_t *bytes,
                                        std::size_t size, std::uint32_t auxiliary_base) const;

  private:
    template <typename SerializedTransform>
    static void ApplyTransform(const SerializedTransform &source, Scene::Transform &target)
    {
        target.position = {source.position[0], source.position[1], source.position[2]};
        target.rotation = {source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]};
        target.scale = {source.scale[0], source.scale[1], source.scale[2]};
        target.UpdateWorldMatrix();
    }

    struct Registration
    {
        std::uint16_t version = 0;
        Loader loader;
    };
    std::unordered_map<std::string, Registration> registrations_;
};

} // namespace CEngine::Entities

#endif
