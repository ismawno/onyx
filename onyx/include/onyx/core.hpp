#pragma once

#include "onyx/alias.hpp"
#include "onyx/dimension.hpp"
#include "graph/core.hpp"
#include "graph/input.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/utils/result.hpp"

#define ONYX_NO_DISCARD [[nodiscard]]

#define ONYX_LOG_RESULT(fun, result) fun(!result, "{}", result.GetError().ToString())
#define ONYX_LOG_EXPRESSION(fun, expression)                                                                           \
    if (const auto __onyx_result = (expression); !__onyx_result)                                                       \
    fun("{}", __onyx_result.GetError().ToString())

#ifdef TKIT_ENABLE_DEBUG_LOGS
#    define ONYX_LOG_RESULT_DEBUG(result) ONYX_LOG_RESULT(TKIT_LOG_DEBUG_IF, (result))
#    define ONYX_LOG_EXPRESSION_DEBUG(expression) ONYX_LOG_EXPRESSION(TKIT_LOG_DEBUG, (expression))
#else
#    define ONYX_LOG_RESULT_DEBUG(result) TKIT_UNUSED(result)
#    define ONYX_LOG_EXPRESSION_DEBUG(expression) expression
#endif

#ifdef TKIT_ENABLE_INFO_LOGS
#    define ONYX_LOG_RESULT_INFO(result) ONYX_LOG_RESULT(TKIT_LOG_INFO_IF, (result))
#    define ONYX_LOG_EXPRESSION_INFO(expression) ONYX_LOG_EXPRESSION(TKIT_LOG_INFO, (expression))
#else
#    define ONYX_LOG_RESULT_INFO(result) TKIT_UNUSED(result)
#    define ONYX_LOG_EXPRESSION_INFO(expression) expression
#endif

#ifdef TKIT_ENABLE_WARNING_LOGS
#    define ONYX_LOG_RESULT_WARNING(result) ONYX_LOG_RESULT(TKIT_LOG_WARNING_IF, (result))
#    define ONYX_LOG_EXPRESSION_WARNING(expression) ONYX_LOG_EXPRESSION(TKIT_LOG_WARNING, (expression))
#else
#    define ONYX_LOG_RESULT_WARNING(result) TKIT_UNUSED(result)
#    define ONYX_LOG_EXPRESSION_WARNING(expression) expression
#endif

#ifdef TKIT_ENABLE_ERROR_LOGS
#    define ONYX_LOG_RESULT_ERROR(result) ONYX_LOG_RESULT(TKIT_LOG_ERROR_IF, (result))
#    define ONYX_LOG_EXPRESSION_ERROR(expression) ONYX_LOG_EXPRESSION(TKIT_LOG_ERROR, (expression))
#else
#    define ONYX_LOG_RESULT_ERROR(result) TKIT_UNUSED(result)
#    define ONYX_LOG_EXPRESSION_ERROR(expression) expression
#endif

#define ONYX_CHECK_RESULT(expression) Onyx::CheckResult(expression);

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
using Graph::Key;
using Graph::Mouse;
using Graph::MouseCursor;
using Graph::PresentMode;

using enum Graph::Key;
using enum Graph::Mouse;
using enum Graph::PresentMode;
using enum Graph::MouseCursor;

static constexpr Graph::Format SurfaceFormat = Graph::Format_B8G8R8A8_UNORM;
enum ErrorCode : u8
{
    Error_LoadFailed,
    Error_FileNotFound,
    Error_EntryPointNotFound,
    Error_ShaderCompilationFailed,
    Error_Unknown,
    Error_Count
};

class Error
{
  public:
    Error() = default;
    Error(const ErrorCode code, const TKit::TierString &message) : m_FormattedMessage(message), m_ErrorCode(code)
    {
    }
    Error(const ErrorCode code, const char *message) : m_CheapMessage(message), m_ErrorCode(code)
    {
    }
    Error(const ErrorCode code) : m_ErrorCode(code)
    {
    }

    TKit::TierString ToString() const;
    TKit::TierString GetMessage() const
    {
        return m_CheapMessage ? m_CheapMessage : m_FormattedMessage;
    }

    ErrorCode GetCode() const
    {
        return m_ErrorCode;
    }

    const char *m_CheapMessage = nullptr;
    TKit::TierString m_FormattedMessage{};
    ErrorCode m_ErrorCode = Error_Unknown;
};

template <typename T = void> using Result = TKit::Result<T, Error>;
using Task = TKit::Task<void>;

template <typename T> T CheckResult(Result<T> &&result)
{
    TKIT_ASSERT(result, "{}", result.GetError().ToString());
    if constexpr (!std::same_as<T, void>)
        return *result;
}

using InitializationFlags = u16;
enum InitializationFlagBit : InitializationFlags
{
    InitializationFlag_DefaultTaskManagerSingleThread = 1U << 0,
    InitializationFlag_EnableDebugUtils = 1U << 1,
    InitializationFlag_EnablePrintfDebugFeature = 1U << 2,
    InitializationFlag_EnableDeviceFaultExtension = 1U << 3,
};

struct Allocation
{
    TKit::ArenaAllocator *Arena = nullptr;
    TKit::StackAllocator *Stack = nullptr;
    TKit::TierAllocator *Tier = nullptr;
};

struct Specs
{
    const char *LoaderPath = nullptr;
    const char *Locale = "en_US.UTF-8";
    // null uses tmp dir. only relevant when device fault extension is enabled
    const char *DeviceFaultCrashDump = nullptr;
    TKit::ITaskManager *TaskManager = nullptr;

    TKit::FixedArray<Allocation, TKit::MaxThreads> Allocators{};
#ifdef TKIT_ENABLE_ENSURE
#    ifdef TKIT_OS_APPLE
    InitializationFlags Flags = InitializationFlag_EnableDebugUtils | InitializationFlag_EnableDeviceFaultExtension;
#    else
    InitializationFlags Flags = InitializationFlag_EnableDebugUtils | InitializationFlag_EnablePrintfDebugFeature |
                                InitializationFlag_EnableDeviceFaultExtension;
#    endif
#else
    InitializationFlags Flags = 0;
#endif
};

void Initialize(const Specs &specs = {});
void Terminate();

const char *ToString(ErrorCode code);

TKit::ArenaAllocator *GetArena(u32 threadIndex);
TKit::StackAllocator *GetStack(u32 threadIndex);
TKit::TierAllocator *GetTier(u32 threadIndex);

void PushArena(u32 threadIndex);
void PushStack(u32 threadIndex);
void PushTier(u32 threadIndex);

// for Pop, you use TKit::Pop.
// NOTE(Isma): Should implement Pop for consistency

TKit::ITaskManager *GetTaskManager();

class Overlay;
template <Dimension D> void Renderer_DisplayMemoryLayout(Overlay *ov);
}; // namespace Onyx
