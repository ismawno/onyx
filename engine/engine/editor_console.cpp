#include "editor_console.hpp"

namespace Engine
{
void SetConsoleLogCallback(Console &console)
{
    TKit::SetLogCallback([&](const TKit::LogInfo &info) {
        if (info.Message.size() > EDITOR_MAX_LOG_ENTRY_MESSAGE)
            return;

        LogEntry entry;
        if (std::strcmp(info.Color, TKIT_LOG_COLOR_INFO) == 0)
            entry.Color = Onyx::Color_Mint;
        else if (std::strcmp(info.Color, TKIT_LOG_COLOR_WARNING) == 0)
            entry.Color = Onyx::Color_Honey;
        else if (std::strcmp(info.Color, TKIT_LOG_COLOR_ERROR) == 0)
            entry.Color = Onyx::Color_Salmon;
        else
            return;

        entry.Message = info.Message;
        entry.Level = info.Level;

        console.Push(entry);
    });
}
} // namespace Engine
