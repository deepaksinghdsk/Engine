#pragma once

#include <stdlib.h>
#include <stdexcept>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "Context.h"

class VulkanSwapChain
{
private:
    VkInstance instance{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    Context *context;

    
public:
    ~VulkanSwapChain();

    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkSwapchainKHR swapChain{VK_NULL_HANDLE}; 
    VkExtent2D swapchainExtent;  
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    std::vector<VkPresentModeKHR> presentModes;
    uint32_t imageCount{0};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};
    std::vector<VkFramebuffer> swapchainFramebuffers;

    void setContext(VkInstance instance, VkDevice device, Context &context);
    void initSurface(GLFWwindow *window);
    void create(int width, int height);
    void createImageViews();
    void createFrameBuffer(VkRenderPass &renderPass);
    void recreateSwapChain(VkRenderPass &renderPass);
    VkResult aquireNextImage();
    VkResult queuePresent();
    void cleanup();
};