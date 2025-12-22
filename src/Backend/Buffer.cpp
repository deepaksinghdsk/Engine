#include "Buffer.h"

Buffer::~Buffer()
{
    destroy();
}

void Buffer::destroy()
{
    vkDestroyBuffer(m_ctx->device, m_buffer, nullptr);
    vkFreeMemory(m_ctx->device, m_memory, nullptr);
}

void Buffer::create(
    const Context &ctx, 
    VkDeviceSize size, 
    VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags memoryFlags)
{
    m_ctx = &ctx;
    m_size = size;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(ctx.device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create vertex buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(ctx.device, m_buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice, memReq.memoryTypeBits, memoryFlags);
    
    if(vkAllocateMemory(ctx.device, &allocInfo, nullptr, &m_memory)!=VK_SUCCESS)
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    vkBindBufferMemory(ctx.device, m_buffer, m_memory, 0);
}

uint32_t Buffer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProp;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProp);

    for(uint32_t i =0; i< memProp.memoryTypeCount; i++)
    {
        if((typeFilter & (1<<i)) && 
            (memProp.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Buffer::upload(const void *data, VkDeviceSize size, VkDeviceSize offset)
{
    void* mapped;
    vkMapMemory(m_ctx->device, m_memory, offset, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(m_ctx->device, m_memory);
}
