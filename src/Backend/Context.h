#define GLFW_INCLUDE_VULKAN
// #define GLFW_STATIC
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan/vulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Context
{
private:
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    GLFWwindow *window;

    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VulkanDevice vulkanDevice;
    VulkanSwapChain *vulkanSwapChain;
    VkQueue graphicsQueue, presentQueue;


    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        // debugMessenger
        return VK_FALSE;
    }

    Context(const Context &other) = delete;
    Context(const Context &&other) = delete;

    Context &operator=(const Context &other) = delete;
    Context &operator=(const Context &&other) = delete;

protected:
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation"};
    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledDeviceExtensions;
    VkPhysicalDeviceFeatures enabledFeatures{};

public:
    uint32_t width = 1280;
    uint32_t height = 720;

    Context();
    ~Context();

    void initVulkan();
    std::vector<const char *> setupExtensions();
    void createInstance();
    void requiredExtensions();
    void createSurface();
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    void createLogicalDevice();
    QueueFamilyIndices Context::findQueueFamilies(VkPhysicalDevice device);
    void setupDebugMessenger();
    bool checkValidationLayerSupport();
};