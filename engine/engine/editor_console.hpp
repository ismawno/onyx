#pragma once

#include "utils.hpp"
#include "onyx/color.hpp"
#include "tkit/container/static_array.hpp"

#define ONYX_MAX_LOG_ENTRIES 64
#define ONYX_MAX_LOG_ENTRY_MESSAGE 256

namespace Engine
{
struct LogEntry
{
    TKit::StaticString<ONYX_MAX_LOG_ENTRY_MESSAGE> Message;
    const char *Level;
    Onyx::Color Color;
};

using ConsoleData = CircularStack<LogEntry, ONYX_MAX_LOG_ENTRY_MESSAGE>;

void SetConsoleLogCallback(ConsoleData &data);
} // namespace Engine
