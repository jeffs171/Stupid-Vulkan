// Utility only -- don't make any VK API calls in here

#include <vulkan/vulkan.h>
#include <string>
#include <iostream>
#include <utility>
#include <type_traits>
#include <optional>

struct SwapChainDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    void print() {
        std::cout << "\tSwap Chain Capabilities:\n";
        std::cout << "\t\tMin ImageCount: " << capabilities.minImageCount << "\n";
        std::cout << "\t\tMax ImageCount: " << capabilities.maxImageCount << "\n";
        std::cout << "\t\tCurrent Extents: {" << capabilities.currentExtent.width << ", " << capabilities.currentExtent.height << "}\n";
        std::cout << "\t\tMin Extents: {" << capabilities.minImageExtent.width << ", " << capabilities.minImageExtent.height << "}\n";
        std::cout << "\t\tMax Extents: {" << capabilities.maxImageExtent.width << ", " << capabilities.maxImageExtent.height << "}\n";
        std::cout << "\t\tMax Image Array Layers: " << capabilities.maxImageArrayLayers << "\n";
        std::cout << "\t\tSupported Transforms: ...\n";
        std::cout << "\t\tCurrent Transforms: ...\n";
        std::cout << "\t\tComposite Alpha: ...\n";
        std::cout << "\t\tUsage Flags: ...\n";
        std::cout << "\tSwap Chain Formats: ...\n";
        std::cout << "\tSwap Chain Present Modes: ...\n";
    }
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isValid() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

void printDevice(
    const VkPhysicalDeviceProperties properties,
    const VkPhysicalDeviceFeatures features) {

    std::cout << "\t" << "API Version: " << properties.apiVersion << "\n";
    std::cout << "\t" << "Driver Version: " << properties.driverVersion << "\n";
    std::cout << "\t" << "Vendor ID: " << properties.vendorID << "\n";
    std::cout << "\t" << "Device ID: " << properties.deviceID << "\n";
    {
        std::string type = "";
        switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            type = "Other";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            type = "Integrated";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            type = "Discrete";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            type = "Virtual";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            type = "CPU";
            break;
        }
        std::cout << "\t" << "Device Type: " << type << "\n";
    }
    std::cout << "\t" << "Device Name: " << properties.deviceName << "\n";
    std::cout << "\t" << "Limits: ...\n";
    std::cout << "\t" << "Sparse Properties: ...\n";

    std::cout << "\t" << "Features: ...\n";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::string severity = "";
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        severity = "V";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        severity = "I";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        severity = "W";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        severity = "E";
        break;
    }

    std::string type = "";
    switch (messageType) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        type = "General";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        type = "Validation";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type = "Performance";
        break;
    }

    std::cout << type << "(" << severity << "): " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                 // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT   | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void printError(VkResult result, char const* const Function, char const* const File, int const Line) {
    if (result != VK_SUCCESS) {
        std::string error;
        switch (result) {
        case VK_NOT_READY:
            error = "VK not ready";
            break;
        case VK_TIMEOUT:
            error = "VK timeout";
            break;
        case VK_EVENT_SET:
            error = "VK event set";
            break;
        case VK_EVENT_RESET:
            error = "VK event reset";
            break;
        case VK_INCOMPLETE:
            error = "VK incomplete";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            error = "VK OOM host";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            error = "VK OOM device";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            error = "VK init failed";
            break;
        case VK_ERROR_DEVICE_LOST:
            error = "VK device lost";
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            error = "VK mmap failed";
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            error = "VK invalid layer";
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            error = "VK invalid extension";
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            error = "VK invalid feature";
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            error = "VK incompatible driver";
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            error = "VK too many objects";
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            error = "VK format not supported";
            break;
        case VK_ERROR_FRAGMENTED_POOL:
            error = "VK fragmented pool";
            break;
        case VK_ERROR_UNKNOWN:
            error = "VK unknown";
            break;
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            error = "VK OOM pool";
            break;
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            error = "VK invalid external handle";
            break;
        case VK_ERROR_FRAGMENTATION:
            error = "VK fragmentation";
            break;
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            error = "VK invalid capture address";
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            error = "VK surface lost";
            break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            error = "VK window in use";
            break;
        case VK_SUBOPTIMAL_KHR:
            error = "VK suboptimal";
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            error = "VK out of date";
            break;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            error = "VK incompatible display";
            break;
        case VK_ERROR_VALIDATION_FAILED_EXT:
            error = "VK validation failed";
            break;
        case VK_ERROR_INVALID_SHADER_NV:
            error = "VK invalid shader";
            break;
        case VK_ERROR_INCOMPATIBLE_VERSION_KHR:
            error = "VK incompatible version";
            break;
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
            error = "VK invalid DRM format";
            break;
        case VK_ERROR_NOT_PERMITTED_EXT:
            error = "VK not permitted";
            break;
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            error = "VK full screen mode lost";
            break;
        case VK_THREAD_IDLE_KHR:
            error = "VK thread idle";
            break;
        case VK_THREAD_DONE_KHR:
            error = "VK thread done";
            break;
        case VK_OPERATION_DEFERRED_KHR:
            error = "VK operation deferred";
            break;
        case VK_OPERATION_NOT_DEFERRED_KHR:
            error = "VK operation not deferred";
            break;
        case VK_PIPELINE_COMPILE_REQUIRED_EXT:
            error = "VK pipeline compile required";
            break;
        }
        std::cerr << "Vulkan error in file " << File << " at line " << Line << " calling function " << Function << ": " << error.c_str() << std::endl;
    }
}

#ifndef CHECK_VK
#define CHECK_VK(x)                                         \
    do {                                                    \
        try {                                               \
            printError((x), #x, __FILE__, __LINE__);        \
        }                                                   \
        catch (const std::exception& e) {                   \
            std::cerr << e.what() << std::endl;             \
        }                                                   \
    } while (0)
#endif
