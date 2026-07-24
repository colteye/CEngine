//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/log.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_LOG_H
#define CENGINE_LOG_H

#include <iostream>
#include <mutex>
#include <string_view>

namespace CEngine::Logging
{

/**
 * @brief TODO: Describe Logger.
 */
class Logger final
{
  public:
    /**
     * @brief TODO: Describe Get.
     *
     * @return TODO: Describe the return value.
     */
    static Logger &Get()
    {
        static Logger logger;
        return logger;
    }

    /**
     * @brief TODO: Describe Info.
     *
     * @param category TODO: Describe this parameter.
     * @param message TODO: Describe this parameter.
     */
    void Info(std::string_view category, std::string_view message)
    {
        Write(std::clog, "info", category, message);
    }

    /**
     * @brief TODO: Describe Warning.
     *
     * @param category TODO: Describe this parameter.
     * @param message TODO: Describe this parameter.
     */
    void Warning(std::string_view category, std::string_view message)
    {
        Write(std::clog, "warning", category, message);
    }

    /**
     * @brief TODO: Describe Error.
     *
     * @param category TODO: Describe this parameter.
     * @param message TODO: Describe this parameter.
     */
    void Error(std::string_view category, std::string_view message)
    {
        Write(std::cerr, "error", category, message);
    }

  private:
    /**
     * @brief TODO: Describe Logger.
     */
    Logger() = default;

    /**
     * @brief TODO: Describe Write.
     *
     * @param stream TODO: Describe this parameter.
     * @param severity TODO: Describe this parameter.
     * @param category TODO: Describe this parameter.
     * @param message TODO: Describe this parameter.
     */
    void Write(std::ostream &stream, std::string_view severity, std::string_view category, std::string_view message)
    {
        const std::scoped_lock lock(mutex_);
        stream << '[' << severity << "][" << category << "] " << message << '\n';
    }

    std::mutex mutex_;
};

} // namespace CEngine::Logging

#endif
