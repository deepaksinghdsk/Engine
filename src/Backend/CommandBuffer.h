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
    void beginCmd( uint32_t currentFrame,
        VkRenderPass &renderPass,
        VkFramebuffer &swapchainFramebuffer,
        VkExtent2D swapchainExtent,
        VkClearColorValue clearColor);
    void endCmd(uint32_t currentFrame);

    void recordCommandBuffer(VkCommandBuffer commandBuffer,
                             VkPipeline &pipeline,
                             VkPipeline &skyboxPipeline, // skybox pipeline
                             VkRenderPass &renderPass,
                             VkFramebuffer &swapchainFramebuffer,
                             VkExtent2D swapchainExtent,
                             VkClearColorValue clearColor,
                             VkBuffer vertexBuffers[], VkDeviceSize offsets[], VkBuffer indexBuffer, VkDeviceSize ibSize,
                             VkBuffer skyboxVertexBuffers[], VkDeviceSize skyboxOffsets[], VkDeviceSize vbSize, // skybox data
                             VkDescriptorSet descSet, VkPipelineLayout pipelineLayout, const std::vector<Model::Submesh> &submeshes,
                             VkDescriptorSet skyboxDescSet, VkPipelineLayout skyboxPipelineLayout, // skybox desc sets
                             const std::unordered_map<uint32_t, int> &matIndTexInd,
                             ImDrawData *drawData);
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