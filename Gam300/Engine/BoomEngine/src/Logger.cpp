#include "Core.h"

namespace Boom {

    std::shared_ptr<spdlog::logger>& GetLogger() {
        // Construct once, in a thread-safe way (C++11 guarantees thread-safe static init)
        static std::shared_ptr<spdlog::logger> logger = [] {
            auto log = spdlog::stdout_color_mt("Boom");
            log->set_level(spdlog::level::trace);
            log->set_pattern("%^[%T] [%n] %v%$");
            return log;
            }();
            return logger;
    }

} // namespace Boom