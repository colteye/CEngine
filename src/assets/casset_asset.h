//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/casset_asset.h
 * @brief Owned reusable object composition.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_CASSET_ASSET_H
#define CENGINE_ASSETS_CASSET_ASSET_H

#include "engine/engine_entities.generated.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace CEngine::Assets
{

inline constexpr std::uint32_t CAssetObjectRoleOccluder = 13;

namespace CAssetWire = Generated::EngineEntities::Wire;
using CAssetObject = CAssetWire::CassetObject;
using CAssetComponent = CAssetWire::CassetComponent;

class CAsset
{
  public:
    bool Load(const std::filesystem::path &path);

    [[nodiscard]] std::uint32_t ObjectCount() const
    {
        return static_cast<std::uint32_t>(data_.objects.size());
    }
    [[nodiscard]] std::uint32_t ComponentCount() const
    {
        return static_cast<std::uint32_t>(data_.components.size());
    }
    [[nodiscard]] std::string_view CollectionName() const
    {
        return data_.name;
    }
    [[nodiscard]] const CAssetObject *Object(std::uint32_t index) const
    {
        return index < data_.objects.size() ? &data_.objects[index] : nullptr;
    }
    [[nodiscard]] const CAssetComponent *Component(std::uint32_t index) const
    {
        return index < data_.components.size() ? &data_.components[index] : nullptr;
    }
    [[nodiscard]] const CAssetComponent *Component(const CAssetObject &object, std::uint32_t local_index) const
    {
        return local_index < object.component_count ? Component(object.first_component + local_index) : nullptr;
    }

  private:
    CAssetWire::Casset data_;
};

} // namespace CEngine::Assets

#endif
