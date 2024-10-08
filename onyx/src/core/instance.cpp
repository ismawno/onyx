#include "core/pch.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/alias.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static DynamicArray<const char *> requiredExtensions() noexcept
{
    u32 glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    KIT_ASSERT(glfwExtensions, "Failed to get required GLFW extensions");

    DynamicArray<const char *> extensions{glfwExtensions, glfwExtensions + glfwExtensionCount};

#ifdef KIT_ENABLE_ASSERTS
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    extensions.insert(extensions.end(), {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                                         "VK_KHR_get_physical_device_properties2"});

    return extensions;
}

#ifdef KIT_ENABLE_ASSERTS

static const char *s_ValidationLayer = "VK_LAYER_KHRONOS_validation";
static VkDebugUtilsMessengerEXT s_DebugMessenger;

static const char *toString(VkDebugUtilsMessageSeverityFlagBitsEXT p_Severity)
{
    switch (p_Severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        return "VERBOSE";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return "ERROR";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return "WARNING";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return "INFO";
    default:
        return "UNKNOWN";
    }
}
static const char *toString(VkDebugUtilsMessageTypeFlagsEXT p_MessageType)
{
    if (p_MessageType == 7)
        return "General | Validation | Performance";
    if (p_MessageType == 6)
        return "Validation | Performance";
    if (p_MessageType == 5)
        return "General | Performance";
    if (p_MessageType == 4)
        return "Performance";
    if (p_MessageType == 3)
        return "General | Validation";
    if (p_MessageType == 2)
        return "Validation";
    if (p_MessageType == 1)
        return "General";
    return "Unknown";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT p_Severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT p_MessageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *p_CallbackData, void *)
{
    KIT_ERROR("<{}: {}> {}", toString(p_Severity), toString(p_MessageType), p_CallbackData->pMessage);
    return VK_FALSE;
}

static VkResult createDebugUtilsMessengerEXT(const VkInstance p_Instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT *p_CreateInfo,
                                             const VkAllocationCallbacks *p_Allocator,
                                             VkDebugUtilsMessengerEXT *p_DebugMessenger) noexcept
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(p_Instance, "vkCreateDebugUtilsMessengerEXT");
    if (func)
        return func(p_Instance, p_CreateInfo, p_Allocator, p_DebugMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void destroyDebugUtilsMessengerEXT(VkInstance p_Instance, const VkAllocationCallbacks *p_Allocator) noexcept
{
    auto func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(p_Instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
        func(p_Instance, s_DebugMessenger, p_Allocator);
}

static VkDebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo() noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional
    return createInfo;
}

static bool checkValidationLayerSupport() noexcept
{
    u32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    DynamicArray<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto &layer_props : availableLayers)
        if (strcmp(s_ValidationLayer, layer_props.layerName) == 0)
            return true;

    return false;
}

static void hasGLFWRequiredInstanceExtensions() noexcept
{
    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    DynamicArray<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    KIT_LOG_INFO("Available instance extensions:");
    HashSet<std::string> available;

    for (const auto &extension : extensions)
    {
        KIT_LOG_INFO("  {}", extension.extensionName);
        available.emplace(extension.extensionName);
    }

    KIT_LOG_INFO("Required instance extensions:");
    const auto reqExtensions = requiredExtensions();
    for (const auto &required : reqExtensions)
    {
        KIT_LOG_INFO("  {}", required);
        KIT_ASSERT(available.contains(required), "Missing required glfw extension");
    }
}

static void setupDebugMessenger(const VkInstance p_Instance) noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo = createDebugMessengerCreateInfo();
    KIT_ASSERT_RETURNS(createDebugUtilsMessengerEXT(p_Instance, &createInfo, nullptr, &s_DebugMessenger), VK_SUCCESS,
                       "Failed to set up debug messenger");
}

const char *Instance::GetValidationLayer() noexcept
{
    return s_ValidationLayer;
}

#endif

/*INSTANCE IMPLEMENTATION*/

Instance::Instance() noexcept
{
    createInstance();
#ifdef KIT_ENABLE_ASSERTS
    setupDebugMessenger(m_Instance);
#endif
}

Instance::~Instance() noexcept
{
    destroyDebugUtilsMessengerEXT(m_Instance, nullptr);
    vkDestroyInstance(m_Instance, nullptr);
}

VkInstance Instance::GetInstance() const noexcept
{
    return m_Instance;
}

void Instance::createInstance() noexcept
{
    KIT_LOG_INFO("Creating a vulkan instance...");
    KIT_ASSERT(checkValidationLayerSupport(), "Validation layers requested, but not available!");
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ONYX App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const auto extensions = requiredExtensions();
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

#ifdef KIT_ENABLE_ASSERTS
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &s_ValidationLayer;

    auto dbgCreateInfo = createDebugMessengerCreateInfo();
    createInfo.pNext = &dbgCreateInfo;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
#endif

    KIT_ASSERT_RETURNS(vkCreateInstance(&createInfo, nullptr, &m_Instance), VK_SUCCESS,
                       "Failed to create vulkan instance");
#ifdef KIT_ENABLE_ASSERTS
    hasGLFWRequiredInstanceExtensions();
#endif
}

} // namespace ONYX