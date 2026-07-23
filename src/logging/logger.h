#ifndef CENGINE_LOGGING_LOGGER_H
#define CENGINE_LOGGING_LOGGER_H

#include <iostream>
#include <mutex>
#include <string_view>

namespace CEngine::Logging {

class Logger final {
public:
    static Logger& Get()
    {
        static Logger logger;
        return logger;
    }

    void Info(std::string_view category, std::string_view message)
    {
        Write(std::clog, "info", category, message);
    }

    void Warning(std::string_view category, std::string_view message)
    {
        Write(std::clog, "warning", category, message);
    }

    void Error(std::string_view category, std::string_view message)
    {
        Write(std::cerr, "error", category, message);
    }

private:
    Logger() = default;

    void Write(std::ostream& stream, std::string_view severity,
        std::string_view category, std::string_view message)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stream << '[' << severity << "][" << category << "] " << message << '\n';
    }

    std::mutex mutex_;
};

} // namespace CEngine::Logging

#endif
