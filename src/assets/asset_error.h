//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/asset_error.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_ASSET_ERROR_H
#define CENGINE_ASSETS_ASSET_ERROR_H

#include "logging/logger.h"

#include <string_view>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe AssetError.
 *
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool AssetError(std::string_view message)
{
    Logging::Logger::Get().Error("assets", message);
    return false;
}

} // namespace CEngine::Assets

#endif
