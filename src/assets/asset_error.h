#ifndef CENGINE_ASSETS_ASSET_ERROR_H
#define CENGINE_ASSETS_ASSET_ERROR_H

#include "logging/logger.h"

#include <string_view>

namespace CEngine::Assets {

inline bool AssetError(std::string_view message)
{
    Logging::Logger::Get().Error("assets", message);
    return false;
}

} // namespace CEngine::Assets

#endif
