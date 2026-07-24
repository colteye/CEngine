//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/casset_asset.cpp
 * @brief Instantiates an owned composition from generated wire data.
 * @author Erik Coltey
 */

#include "assets/casset_asset.h"

#include "log.h"

#include <unordered_set>

namespace CEngine::Assets
{

bool CAsset::Load(const std::filesystem::path &path)
{
    CAssetWire::Casset decoded;
    if (!Decode(path, Type::Asset, decoded))
    {
        Logging::Logger::Get().Error("assets", "casset payload is invalid");
        return false;
    }

    std::unordered_set<std::string> names;
    for (std::size_t index = 0; index < decoded.objects.size(); ++index)
    {
        const CAssetWire::CassetObject &object = decoded.objects[index];
        if (!names.insert(object.name).second || (index == 0u && object.parent != -1) ||
            (object.parent >= 0 && static_cast<std::size_t>(object.parent) >= index) ||
            object.first_component > decoded.components.size() ||
            object.component_count > decoded.components.size() - object.first_component)
        {
            Logging::Logger::Get().Error("assets", "casset object hierarchy is invalid");
            return false;
        }
    }

    data_ = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
