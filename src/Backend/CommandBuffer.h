#pragma once
#include "Context.h"
#include "RenderPass.h"
#include "Model.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include <unordered_map>

class CommandBuffer
{
public:
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandBuffer commandBuffer;

    CommandBuffer(Context &context, const int framesInFlight);
    ~CommandBuffer();

    void createCommandPool();

    void beginRenderPass(uint32_t currentFrame,
                         VkRenderPass &renderPass,
                         VkFramebuffer &framebuffer,
                         VkExtent2D extent,
                         VkClearColorValue clearColor);
    void endRenderPass(uint32_t currentFrame);

    void beginCmdbuffer(uint32_t currentFrame);
    void endCmdbuffer(uint32_t currentFrame);

    VkCommandPool getCmdPool() const { return commandPool; }

    struct DrawData
    {
        uint32_t texIndex;
    };

private:
    VkCommandPool commandPool;

    Context &context; /*
    RenderPass &renderPass;
 VulkanSwapChain &swapchain; */
};