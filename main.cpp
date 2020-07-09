#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <cstdint> 

#include "vkHelper.hpp"

class HelloTriangleApplication {
public:
    void run() {
#ifdef _DEBUG
        mEnableValidationLayers = true;
#endif
        init();
        mainLoop();
        cleanup();
    }

private:
    void init() {
        // GLFW
        {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            mWindow = glfwCreateWindow(sResolution.x, sResolution.y, "Vulkan", nullptr, nullptr);
        }

        // Instance
        {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Hello Triangle";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            uint32_t extensionCount = 0;
            const char** extensionNames;
            extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
            std::vector<const char*> extensions(extensionNames, extensionNames + extensionCount);
            if (mEnableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            // Validation layers
            {
                // Validate validation layers
                if (mEnableValidationLayers) {
                    uint32_t layerCount;
                    CHECK_VK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
                    std::vector<VkLayerProperties> availableLayers(layerCount);
                    CHECK_VK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));
                    for (const char* layerName : sValidationLayers) {
                        bool layerFound = false;
                        for (const auto& properties : availableLayers) {
                            if (strcmp(layerName, properties.layerName) == 0) {
                                layerFound = true;
                                break;
                            }
                        }
                        if (!layerFound) {
                            throw std::runtime_error("Unable to find validation layer");
                        }
                    }
                }

                VkDebugUtilsMessengerCreateInfoEXT debugInfo;
                if (mEnableValidationLayers) {
                    populateDebugMessengerCreateInfo(debugInfo);
                    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;
                    createInfo.enabledLayerCount = static_cast<uint32_t>(sValidationLayers.size());
                    createInfo.ppEnabledLayerNames = sValidationLayers.data();
                }
                else {
                    createInfo.enabledLayerCount = 0;
                    createInfo.pNext = nullptr;
                }
            }

            CHECK_VK(vkCreateInstance(&createInfo, nullptr, &mInstance));
        }

        // Extensions
        {
            uint32_t extensionCount = 0;
            CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
            std::vector<VkExtensionProperties> extensions(extensionCount);
            CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()));
            std::cout << "Extensions: \n";
            for (const auto& extension : extensions) {
                std::cout << "\t" << extension.extensionName << " v" << extension.specVersion << "\n";
            }
            std::cout << std::endl;
        }

        // Debug Messenger 
        if (mEnableValidationLayers) {
            VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
            populateDebugMessengerCreateInfo(debugInfo);

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
            if (func) {
                CHECK_VK(func(mInstance, &debugInfo, nullptr, &mDebugMessenger));
            }
            else {
                throw std::runtime_error("failed to set up debug messenger");
            }
        }

        // Surface
        {
            CHECK_VK(glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface));
        }

        // Physical Device
        {
            uint32_t deviceCount = 0;
            CHECK_VK(vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr));
            if (deviceCount == 0) {
                throw std::runtime_error("Couldn't find a physical device");
            }
            std::vector<VkPhysicalDevice> devices(deviceCount);
            CHECK_VK(vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data()));

            for (int i = 0; i < devices.size(); i++) {
                VkPhysicalDevice device = devices[i];
                VkPhysicalDeviceProperties properties;
                VkPhysicalDeviceFeatures features;
                vkGetPhysicalDeviceProperties(device, &properties);
                vkGetPhysicalDeviceFeatures(device, &features);

                std::cout << "\nDevice [" << i << "]\n";
                printDevice(properties, features);


                // Discrete
                bool isDiscrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

                // Extensions
                bool extensionSupported = false;
                {
                    uint32_t extensionCount;
                    CHECK_VK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));
                    std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
                    CHECK_VK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, deviceExtensions.data()));

                    std::set<std::string> requiredExtensions(sPhysicalDeviceExtensions.begin(), sPhysicalDeviceExtensions.end());
                    for (const VkExtensionProperties& extension : deviceExtensions) {
                        requiredExtensions.erase(extension.extensionName);
                    }

                    extensionSupported = requiredExtensions.empty();
                }

                // Swap Chain
                bool viableSwapChain = false;
                {
                    if (extensionSupported) {
                        SwapChainDetails details = getSwapChainDetails(device);
                        details.print();
                        viableSwapChain = !details.formats.empty() && !details.presentModes.empty();
                    }
                }

                // Queues
                QueueFamilyIndices indices = getQueueIndices(device);

                if (!isDiscrete) {
                    std::cout << "\tInvalid: Not discrete GPU" << std::endl;
                }
                if (!extensionSupported) {
                    std::cout << "\tInvalid: Extensions unsupported" << std::endl;
                }
                if (!viableSwapChain) {
                    std::cout << "\tInvalid: SwapChain unviable" << std::endl;
                }
                if (!indices.isValid()) {
                    if (!indices.graphicsFamily.has_value()) {
                        std::cout << "\tInvalid: Graphics Queue unsupported" << std::endl;
                    }
                    if (!indices.presentFamily.has_value()) {
                        std::cout << "\tInvalid: Present Queue unsupported" << std::endl;
                    }
                }
                if (isDiscrete && extensionSupported && viableSwapChain && indices.isValid())
                {
                    std::cout << "\tValid!" << std::endl;
                    mPhysicalDevice = device;
                }
            }
            assert(mPhysicalDevice != VK_NULL_HANDLE);
        }

        // Logical Device & Queue
        {
            QueueFamilyIndices indices = getQueueIndices(mPhysicalDevice);

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            float queuePriority = 1.0f;
            for (uint32_t family : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = family;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures = {};
            VkDeviceCreateInfo deviceInfo = {};
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
            deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            deviceInfo.pEnabledFeatures = &deviceFeatures;
            deviceInfo.enabledExtensionCount = static_cast<uint32_t>(sPhysicalDeviceExtensions.size());
            deviceInfo.ppEnabledExtensionNames = sPhysicalDeviceExtensions.data();
            if (mEnableValidationLayers) {
                deviceInfo.enabledLayerCount = static_cast<uint32_t>(sValidationLayers.size());
                deviceInfo.ppEnabledLayerNames = sValidationLayers.data();
            }
            else {
                deviceInfo.enabledLayerCount = 0;
            }

            CHECK_VK(vkCreateDevice(mPhysicalDevice, &deviceInfo, nullptr, &mLogicalDevice));

            vkGetDeviceQueue(mLogicalDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
            vkGetDeviceQueue(mLogicalDevice, indices.graphicsFamily.value(), 0, &mPresentQueue);
        }

        // Swap Chain
        {
            SwapChainDetails swapChainDetails = getSwapChainDetails(mPhysicalDevice);

            mSwapChainSurfaceFormat = swapChainDetails.formats[0];
            for (const VkSurfaceFormatKHR& availableFormat : swapChainDetails.formats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    mSwapChainSurfaceFormat = availableFormat;
                }
            }

            VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
            for (const VkPresentModeKHR& availablePresentMode : swapChainDetails.presentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    presentMode = availablePresentMode;
                }
            }

            mSwapChainExtent = { UINT32_MAX, UINT32_MAX };
            if (swapChainDetails.capabilities.currentExtent.width != UINT32_MAX) {
                mSwapChainExtent = swapChainDetails.capabilities.currentExtent;
            }
            else {
                mSwapChainExtent.width = std::max(swapChainDetails.capabilities.minImageExtent.width, std::min(swapChainDetails.capabilities.maxImageExtent.width, mSwapChainExtent.width));
                mSwapChainExtent.height = std::max(swapChainDetails.capabilities.minImageExtent.height, std::min(swapChainDetails.capabilities.maxImageExtent.height, mSwapChainExtent.height));
            }

            uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;
            if (swapChainDetails.capabilities.maxImageCount > 0 && imageCount > swapChainDetails.capabilities.maxImageCount) {
                imageCount = swapChainDetails.capabilities.maxImageCount;
            }

            QueueFamilyIndices indices = getQueueIndices(mPhysicalDevice);
            uint32_t indicesArr[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            VkSwapchainCreateInfoKHR swapChainCreateInfo{};
            swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapChainCreateInfo.surface = mSurface;
            swapChainCreateInfo.minImageCount = imageCount;
            swapChainCreateInfo.imageFormat = mSwapChainSurfaceFormat.format;
            swapChainCreateInfo.imageColorSpace = mSwapChainSurfaceFormat.colorSpace;
            swapChainCreateInfo.imageExtent = mSwapChainExtent;
            swapChainCreateInfo.imageArrayLayers = 1;
            swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            swapChainCreateInfo.imageSharingMode = indicesArr[0] == indicesArr[1] ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
            swapChainCreateInfo.queueFamilyIndexCount = indicesArr[0] == indicesArr[1] ? 0 : 2;
            swapChainCreateInfo.pQueueFamilyIndices = indicesArr[0] == indicesArr[1] ? nullptr : indicesArr;
            swapChainCreateInfo.preTransform = swapChainDetails.capabilities.currentTransform;
            swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // TODO - change this lol
            swapChainCreateInfo.presentMode = presentMode;
            swapChainCreateInfo.clipped = VK_TRUE;
            swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

            CHECK_VK(vkCreateSwapchainKHR(mLogicalDevice, &swapChainCreateInfo, nullptr, &mSwapChain));

            uint32_t swapChainImages = 0;
            CHECK_VK(vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &swapChainImages, nullptr));
            mSwapChainImages.resize(swapChainImages);
            CHECK_VK(vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &swapChainImages, mSwapChainImages.data()));
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(mWindow)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroySwapchainKHR(mLogicalDevice, mSwapChain, nullptr);
        vkDestroyDevice(mLogicalDevice, nullptr);

        // Debug Messenger
        if (mEnableValidationLayers) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) {
                func(mInstance, mDebugMessenger, nullptr);
            }
            else {
                throw std::runtime_error("Failed to release debug messenger");
            }
        }

        // TODO - CHECK_VK
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }

    QueueFamilyIndices getQueueIndices(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isValid()) {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainDetails getSwapChainDetails(VkPhysicalDevice device) {
        SwapChainDetails details;

        CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.capabilities));

        uint32_t formatCount;
        CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr));
        if (formatCount) {
            details.formats.resize(formatCount);
            CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.formats.data()));
        }

        uint32_t presentModeCount;
        CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr));
        if (presentModeCount) {
            details.presentModes.resize(presentModeCount);
            CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.presentModes.data()));
        }

        return details;
    }

    // Window things
    GLFWwindow* mWindow = nullptr;
    const glm::ivec2 sResolution = { 1024, 1024 };

    // Vk things
    VkInstance mInstance;

    const std::vector<const char*> sValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    bool mEnableValidationLayers = false;

    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkSurfaceKHR mSurface;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    const std::vector<const char*> sPhysicalDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDevice mLogicalDevice;

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;

    VkSwapchainKHR mSwapChain;
    std::vector<VkImage> mSwapChainImages;
    VkSurfaceFormatKHR mSwapChainSurfaceFormat;
    VkExtent2D mSwapChainExtent;
};

int main() {
    HelloTriangleApplication app;

    app.run();

    return EXIT_SUCCESS;
}
