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

void CommandBuffer::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                        VkPipeline &pipeline,
                                        VkRenderPass &renderPass,
                                        VkFramebuffer &swapchainFramebuffer,
                                        VkExtent2D swapchainExtent,
                                        VkClearColorValue clearColor,
                                        VkBuffer vertexBuffers[], VkDeviceSize offsets[], VkBuffer indexBuffer, VkDeviceSize ibSize,
                                        VkDescriptorSet descSet, VkPipelineLayout pipelineLayout,
                                        ImDrawData *drawData)
{
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = clearColor;//{{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);
    // vkCmdDraw(commandBuffer, static_cast<uint32_t>(vbSize), 1, 0, 0);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(ibSize), 1, 0, 0, 0);
    
    ImGui_ImplVulkan_RenderDrawData(
        ImGui::GetDrawData(),
        commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
    
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer!");
}