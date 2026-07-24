//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/entity_factory.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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

/**
 * @brief TODO: Describe EntityFactory.
 */
class EntityFactory
{
  public:
    using Loader = std::function<std::unique_ptr<Scene::Entity>(const std::uint8_t *, std::size_t, std::uint32_t)>;

    /**
     * @brief TODO: Describe EntityFactory.
     */
    EntityFactory();

    /**
     * @brief TODO: Describe Register.
     *
     * @tparam EntityType TODO: Describe this template parameter.
     * @tparam SerializedType TODO: Describe this template parameter.
     * @tparam Args TODO: Describe this template parameter.
     * @param classname TODO: Describe this parameter.
     * @param args TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
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

    /**
     * @brief TODO: Describe Register.
     *
     * @param classname TODO: Describe this parameter.
     * @param version TODO: Describe this parameter.
     * @param loader TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Register(std::string classname, std::uint16_t version, Loader loader);
    /**
     * @brief TODO: Describe Load.
     *
     * @param classname TODO: Describe this parameter.
     * @param version TODO: Describe this parameter.
     * @param bytes TODO: Describe this parameter.
     * @param size TODO: Describe this parameter.
     * @param auxiliary_base TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::unique_ptr<Scene::Entity> Load(std::string_view classname, std::uint16_t version, const std::uint8_t *bytes,
                                        std::size_t size, std::uint32_t auxiliary_base) const;

  private:
    /**
     * @brief TODO: Describe ApplyTransform.
     *
     * @tparam SerializedTransform TODO: Describe this template parameter.
     * @param source TODO: Describe this parameter.
     * @param target TODO: Describe this parameter.
     */
    template <typename SerializedTransform>
    static void ApplyTransform(const SerializedTransform &source, Scene::Transform &target)
    {
        target.position = {source.position[0], source.position[1], source.position[2]};
        target.rotation = {source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]};
        target.scale = {source.scale[0], source.scale[1], source.scale[2]};
        target.UpdateWorldMatrix();
    }

    /**
     * @brief TODO: Describe Registration.
     */
    struct Registration
    {
        std::uint16_t version = 0;
        Loader loader;
    };
    std::unordered_map<std::string, Registration> registrations_;
};

} // namespace CEngine::Entities

#endif
