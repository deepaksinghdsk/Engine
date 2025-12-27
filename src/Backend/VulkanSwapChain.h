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
#include "Image.h"
#include "CommandBuffer.h"

class VulkanSwapChain
{ 
public:
    ~VulkanSwapChain();

    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkSurfaceFormatKHR selectedFormat;
    VkSwapchainKHR swapChain{VK_NULL_HANDLE}; 
    VkExtent2D swapchainExtent;  
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    VkPresentModeKHR swapchainPresentMode;
    uint32_t imageCount{0};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};
    std::vector<VkFramebuffer> swapchainFramebuffers;

    void setContext(VkInstance instance, VkDevice device, Context &context);
    void initSurface(GLFWwindow *window);
    void create(int width, int height, CommandBuffer* cmdBuffer);
    void createImageViews();
    void createDepthImage();
    void createFrameBuffer(VkRenderPass &renderPass);
    void recreateSwapChain(VkRenderPass &renderPass);
    VkResult aquireNextImage();
    VkResult queuePresent();

    Image* getDepthImage() const {return depthImage;} 

    void cleanup();

private:
    VkInstance instance{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    Image *depthImage;
    CommandBuffer* m_cmdBuffer;

    Context *context;
};