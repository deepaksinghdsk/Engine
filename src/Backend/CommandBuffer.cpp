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

void CommandBuffer::beginRenderPass(uint32_t currentFrame,
                                    VkRenderPass &renderPass,
                                    VkFramebuffer &framebuffer,
                                    VkExtent2D extent,
                                    VkClearColorValue clearColor)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = clearColor; //{{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::endRenderPass(uint32_t currentFrame)
{
    vkCmdEndRenderPass(commandBuffers[currentFrame]);
}

void CommandBuffer::beginCmdbuffer(uint32_t currentFrame)
{
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer");
}

void CommandBuffer::endCmdbuffer(uint32_t currentFrame)
{
    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer!");
}