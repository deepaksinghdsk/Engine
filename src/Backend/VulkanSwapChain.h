#pragma once

#include <stdlib.h>
#include <stdexcept>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

class VulkanSwapChain
{
private:
    VkInstance instance{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};

public:
    VkSwapchainKHR swapChain{VK_NULL_HANDLE};   
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    std::vector<VkPresentModeKHR> presentModes;
    uint32_t imageCount{0};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};

    void setContext(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice);
    void initSurface(GLFWwindow *window);
    void create(int width, int height);
    VkResult aquireNextImage();
    VkResult queuePresent();
    void cleanup();
};