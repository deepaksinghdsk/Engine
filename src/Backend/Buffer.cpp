#include "Buffer.h"
#include "Context.h"

Buffer::~Buffer()
{
    destroy();
}

void Buffer::destroy()
{
    if (!m_ctx)
        return;

    if (mapped)
    {
        vkUnmapMemory(m_ctx->device, m_memory);
        mapped = nullptr;
    }

    if (m_buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_ctx->device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_ctx->device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

void Buffer::create(
    const Context &ctx,
    VkDeviceSize size, VkDeviceSize offset,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memoryFlags)
{
    m_ctx = &ctx;
    m_size = size;
    m_memoryFlags = memoryFlags;

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(ctx.device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create vertex buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(ctx.device, m_buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice, memReq.memoryTypeBits, memoryFlags);

    if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    vkBindBufferMemory(ctx.device, m_buffer, m_memory, 0);

    // Persistent mapping - only for host-visible memory
    if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vkMapMemory(m_ctx->device, m_memory, offset, m_size, 0, &mapped);
    }
}

uint32_t Buffer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProp;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProp);

    for (uint32_t i = 0; i < memProp.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProp.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Buffer::upload(const void *data, bool persistMap, uint32_t face, size_t faceSize)
{
    if (!mapped)
    {
        // Map memory if not already mapped
        vkMapMemory(m_ctx->device, m_memory, 0, m_size, 0, &mapped);
    }

    if (!faceSize)
    {
        memcpy(mapped, data, (size_t)m_size);
    }
    else
    {
        uint8_t *dst = reinterpret_cast<uint8_t *>(mapped);
        memcpy(
            dst + face * faceSize,
            data,
            faceSize);
    }

    if (!persistMap && (m_memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
    {
        vkUnmapMemory(m_ctx->device, m_memory);
        mapped = nullptr;
    }
}

void Buffer::copyBuffer(const VkBuffer &srcBuffer, VkDeviceSize size, VkCommandPool cmdPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = cmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_ctx->device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, m_buffer, 1, &copyRegion);
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    vkQueueSubmit(m_ctx->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_ctx->graphicsQueue);

    vkFreeCommandBuffers(m_ctx->device, cmdPool, 1, &cmdBuffer);
}
