#pragma once

#include "utils.hpp"
#include "onyx/color.hpp"
#include "tkit/container/static_array.hpp"

#define EDITOR_MAX_LOG_ENTRIES 64
#define EDITOR_MAX_LOG_ENTRY_MESSAGE 256

namespace Engine
{
struct LogEntry
{
    TKit::StaticString<EDITOR_MAX_LOG_ENTRY_MESSAGE> Message;
    const char *Level;
    Onyx::Color Color;
};

using Console = CircularStack<LogEntry, EDITOR_MAX_LOG_ENTRY_MESSAGE>;

void SetConsoleLogCallback(Console &console);
} // namespace Engine
