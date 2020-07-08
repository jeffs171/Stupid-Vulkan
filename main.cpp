#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "vkHelper.hpp"

class HelloTriangleApplication {
public:
    void run() {
#ifdef _DEBUG
        enableValidationLayers = true;
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
            window = glfwCreateWindow(resolution.x, resolution.y, "Vulkan", nullptr, nullptr);
        }

        // Instance
        {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Hello Triangle";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            uint32_t extensionCount = 0;
            const char** extensionNames;
            extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
            std::vector<const char*> extensions(extensionNames, extensionNames + extensionCount);
            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = &extensions[0];
            createInfo.enabledLayerCount = 0;

            // Validation layers
            {
                uint32_t layerCount;
                CHECK_VK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
                std::vector<VkLayerProperties> availableLayers(layerCount);
                CHECK_VK(vkEnumerateInstanceLayerProperties(&layerCount, &availableLayers[0]));
                for (const char* layerName : validationLayers) {
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

                VkDebugUtilsMessengerCreateInfoEXT debugInfo;
                if (enableValidationLayers) {
                    populateDebugMessengerCreateInfo(debugInfo);
                    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                    createInfo.ppEnabledLayerNames = &validationLayers[0];
                    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;
                }
                else {
                    createInfo.enabledLayerCount = 0;
                    createInfo.pNext = nullptr;
                }
            }

            CHECK_VK(vkCreateInstance(&createInfo, nullptr, &instance));
        }

        // Extensions
        {
            uint32_t extensionCount = 0;
            CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
            std::vector<VkExtensionProperties> extensions(extensionCount);
            CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, &extensions[0]));
            std::cout << "Extensions: \n";
            for (const auto& extension : extensions) {
                std::cout << "\t" << extension.extensionName << " v" << extension.specVersion << "\n";
            }
            std::cout << std::endl;
        }

        // Debug Messenger 
        if (enableValidationLayers) {
            VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
            populateDebugMessengerCreateInfo(debugInfo);

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
            if (func) {
                CHECK_VK(func(instance, &debugInfo, nullptr, &debugMessenger));
            }
            else {
                throw std::runtime_error("failed to set up debug messenger");
            }
        }

        // Physical Device
        {
            uint32_t deviceCount = 0;
            CHECK_VK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
            if (deviceCount == 0) {
                throw std::runtime_error("Couldn't find a physical device");
            }
            std::vector<VkPhysicalDevice> devices(deviceCount);
            CHECK_VK(vkEnumeratePhysicalDevices(instance, &deviceCount, &devices[0]));

            physicalDevice = devices[0];
            for (int i = 0; i < devices.size(); i++) {
                VkPhysicalDevice device = devices[i];
                VkPhysicalDeviceProperties properties;
                VkPhysicalDeviceFeatures features;
                vkGetPhysicalDeviceProperties(device, &properties);
                vkGetPhysicalDeviceFeatures(device, &features);
                printDevice(i, properties, features);

                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, &queueFamilies[0]);

                bool viable = true;

                if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    viable = false;
                }
                for (const auto& family : queueFamilies) {
                    if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT == 0) {
                        viable = false;
                        break;
                    }
                }

                if (viable) {
                    physicalDevice = device;
                }
            }
        }

        // Logical Device & Queue
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, &queueFamilies[0]);
            int graphicsFamily = 0;
            for (auto family : queueFamilies) {
                if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    break;
                }
                graphicsFamily++;
            }

            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = graphicsFamily;
            queueInfo.queueCount = 1;
            float queuePriority = 1.f;
            queueInfo.pQueuePriorities = &queuePriority;

            VkPhysicalDeviceFeatures deviceFeatures = {};
            
            VkDeviceCreateInfo deviceInfo = {};
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.pQueueCreateInfos = &queueInfo;
            deviceInfo.queueCreateInfoCount = 1;
            deviceInfo.pEnabledFeatures = &deviceFeatures;
            deviceInfo.enabledExtensionCount = 0;
            if (enableValidationLayers) {
                deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                deviceInfo.ppEnabledLayerNames = validationLayers.data();
            }
            else {
                deviceInfo.enabledLayerCount = 0;
            }

            CHECK_VK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &logicalDevice));

            vkGetDeviceQueue(logicalDevice, graphicsFamily, 0, &graphicsQueue);
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyDevice(logicalDevice, nullptr);

        // Debug Messenger
        if (enableValidationLayers) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) {
                func(instance, debugMessenger, nullptr);
            }
            else {
                throw std::runtime_error("Failed to release debug messenger");
            }
        }

        // TODO - CHECK_VK
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // Window things
    GLFWwindow* window = nullptr;
    const glm::ivec2 resolution = { 1024, 1024 };

    // Vk things
    VkInstance instance;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    bool enableValidationLayers = false;

    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice;

    VkQueue graphicsQueue;
};

int main() {
    HelloTriangleApplication app;

    app.run();

    return EXIT_SUCCESS;
}
