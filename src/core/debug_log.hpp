#pragma once

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

namespace core
{

inline void debugLog(
    const char *hypothesisId,
    const char *location,
    const char *message,
    const std::string &dataJson = "{}"
)
{
    // #region agent log
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    std::ostringstream line;
    line << "{\"sessionId\":\"16bad8\",\"hypothesisId\":\"" << hypothesisId
         << "\",\"location\":\"" << location << "\",\"message\":\"" << message
         << "\",\"data\":" << dataJson << ",\"timestamp\":" << ms << "}\n";

    std::ofstream f("debug-16bad8.log", std::ios::app);
    if (!f) {
        f.open("build/Debug/debug-16bad8.log", std::ios::app);
    }
    if (f) {
        f << line.str();
    }
    // #endregion
}

} // namespace core
