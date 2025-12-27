#pragma once
#include "vulkan/vulkan.h"

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

// Forward declaration
struct GLFWwindow;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Context
{
public:
    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance;
    VulkanDevice *vulkanDevice;
    GLFWwindow *window;
    VkQueue graphicsQueue, presentQueue;
    VkSurfaceKHR surface;

    bool framebufferResized = false;

private:
    Context(const Context &other) = delete;
    Context(const Context &&other) = delete;
    
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        // debugMessenger
        return VK_FALSE;
    }

    Context &operator=(const Context &other) = delete;
    Context &operator=(const Context &&other) = delete;

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

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void initVulkan();
    // std::vector<const char *> setupExtensions();
    void createInstance();
    void requiredExtensions();
    void createSurface();
    void createSwapchain();
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    void createLogicalDevice();
    void setupDebugMessenger();
    bool checkValidationLayerSupport();
};