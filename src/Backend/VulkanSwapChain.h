#pragma once

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <vulkan/vulkan.h>

class VulkanSwapChain
{
private:
    VkInstance instance{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};

public:
    VkSwapchainKHR swapChain{VK_NULL_HANDLE};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};
    uint32_t imageCount{0};

    void setContext(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice);
    void initSurface();
    void createSwapChain();
    void cleanup();
};