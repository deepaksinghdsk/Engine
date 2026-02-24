#include "CommandBuffer.h"

CommandBuffer::CommandBuffer(Context &context, const int framesInFlight) : context(context)
{
    createCommandPool();

    commandBuffers.resize(framesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(context.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
}

CommandBuffer::~CommandBuffer()
{
    vkDestroyCommandPool(context.device, commandPool, nullptr);
}

void CommandBuffer::createCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = context.vulkanDevice->queueFamilyIndices.graphics.value();

    if (vkCreateCommandPool(context.device, &cmdPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool");
}

void CommandBuffer::beginCmd( uint32_t currentFrame,
    VkRenderPass &renderPass,
    VkFramebuffer &swapchainFramebuffer,
    VkExtent2D swapchainExtent,
    VkClearColorValue clearColor)
{
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = clearColor; //{{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::endCmd(uint32_t currentFrame)
{
    vkCmdEndRenderPass(commandBuffers[currentFrame]);

    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer!");
}

void CommandBuffer::recordCommandBuffer(VkCommandBuffer commandBuffer,
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
                                        ImDrawData *drawData)
{
 
    
}