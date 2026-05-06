#pragma once

#include "onyx/alias.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/utils/result.hpp"

#define ONYX_NO_DISCARD [[nodiscard]]
#define ONYX_CHECK_RESULT(expression) Onyx::CheckResult(expression);

#define ONYX_DECLARE_DISPATCHABLE_VK_HANDLE(name)                                                                      \
    struct name##_T;                                                                                                   \
    using name = name##_T *;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) ||               \
    defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#    define ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE ONYX_DECLARE_DISPATCHABLE_VK_HANDLE
#else
#    define ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE(name) using name = u64;
#endif

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
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
    Error(const ErrorCode code, const std::string &message) : m_FormattedMessage(message), m_ErrorCode(code)
    {
    }
    Error(const ErrorCode code, const char *message) : m_CheapMessage(message), m_ErrorCode(code)
    {
    }
    Error(const ErrorCode code) : m_ErrorCode(code)
    {
    }

    std::string ToString() const;
    std::string GetMessage() const
    {
        return m_CheapMessage ? m_CheapMessage : m_FormattedMessage;
    }

    ErrorCode GetCode() const
    {
        return m_ErrorCode;
    }

    const char *m_CheapMessage = nullptr;
    std::string m_FormattedMessage{};
    ErrorCode m_ErrorCode = Error_Unknown;
};

template <typename T = void> using Result = TKit::Result<T, Error>;
using Task = TKit::Task<void>;

template <typename T> T CheckResult(Result<T> &&result)
{
    TKIT_ASSERT(result, "{}", result.GetError().ToString());
    if constexpr (!std::same_as<T, void>)
        return result.GetValue();
}

namespace Execution
{
struct Specs;
} // namespace Execution
namespace Resources
{
struct Specs;
} // namespace Resources
namespace Descriptors
{
struct Specs;
} // namespace Descriptors

#ifdef ONYX_ENABLE_SHADER_API
namespace Shaders
{
struct Specs;
} // namespace Shaders
#endif

namespace Renderer
{
struct Specs;
} // namespace Renderer
namespace Platform
{
struct Specs;
} // namespace Platform

using InitializationFlags = u16;
enum InitializationFlagBit : InitializationFlags
{
    InitializationFlag_DefaultTaskManagerSingleThread = 1 << 0,
    InitializationFlag_EnableValidationLayers = 1 << 1,
    InitializationFlag_EnableDebugUtilsExtension = 1 << 2,
    InitializationFlag_EnableDeviceAssistedDebugFeature = 1 << 3,
    InitializationFlag_EnableBestPracticesDebugFeature = 1 << 4,
    InitializationFlag_EnablePrintfDebugFeature = 1 << 5,
    InitializationFlag_EnableSyncValidationDebugFeature = 1 << 6,
    InitializationFlag_EnableDeviceFaultExtension = 1 << 7,
    InitializationFlag_ResetArenasOnTermination = 1 << 8
};

struct Allocation
{
    TKit::ArenaAllocator *Arena = nullptr;
    TKit::StackAllocator *Stack = nullptr;
    TKit::TierAllocator *Tier = nullptr;
};

struct Specs
{
    const char *VulkanLoaderPath = nullptr;
    const char *Locale = "en_US.UTF-8";
    // null uses tmp dir. only relevant when device fault extension is enabled
    const char *DeviceFaultCrashDump = nullptr;
    TKit::ITaskManager *TaskManager = nullptr;

    u32 GraphicsQueueCount = 1;
    u32 TransferQueueCount = 1;
    u32 PresentQueueCount = 1;

    TKit::FixedArray<Allocation, TKit::MaxThreads> Allocators{};
    Execution::Specs *ExecutionSpecs = nullptr;
    Resources::Specs *ResourceSpecs = nullptr;
    Descriptors::Specs *DescriptorSpecs = nullptr;
#ifdef ONYX_ENABLE_SHADER_API
    Shaders::Specs *ShadersSpecs = nullptr;
#endif
    Renderer::Specs *RendererSpecs = nullptr;
    Platform::Specs *PlatformSpecs = nullptr;
#ifdef TKIT_ENABLE_ASSERTS
#    ifdef TKIT_OS_APPLE
    InitializationFlags Flags =
        InitializationFlag_EnableValidationLayers | InitializationFlag_EnableDebugUtilsExtension |
        InitializationFlag_EnableBestPracticesDebugFeature | InitializationFlag_EnableSyncValidationDebugFeature |
        InitializationFlag_EnableDeviceFaultExtension;
#    else
    InitializationFlags Flags =
        InitializationFlag_EnableValidationLayers | InitializationFlag_EnableDebugUtilsExtension |
        InitializationFlag_EnableBestPracticesDebugFeature | InitializationFlag_EnableSyncValidationDebugFeature |
        InitializationFlag_EnablePrintfDebugFeature | InitializationFlag_EnableDeviceFaultExtension;
#    endif
#else
    InitializationFlags Flags = 0;
#endif
};

void Initialize(const Specs &specs = {});
void Terminate();

const char *ToString(ErrorCode code);

TKit::ArenaAllocator *GetArena(u32 threadIndex = 0);
TKit::StackAllocator *GetStack(u32 threadIndex = 0);
TKit::TierAllocator *GetTier(u32 threadIndex = 0);

void PushArena(u32 threadIndex = 0);
void PushStack(u32 threadIndex = 0);
void PushTier(u32 threadIndex = 0);

// for Pop, you use TKit::Pop.
// NOTE(Isma): Should implement Pop for consistency

TKit::ITaskManager *GetTaskManager();
}; // namespace Onyx
