#pragma once
#include <vulkan/vulkan.h>
#include "Application.h"

class Buffer
{
public:
    Buffer() = default;
    ~Buffer();

    //Non-copyable, movable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void create(
        const Context& ctx,
        VkDeviceSize size, VkDeviceSize offset,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags
    );

    void destroy();
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void upload(const void* data, bool persistMap);
    void copyBuffer(const VkBuffer& srcBuffer, VkDeviceSize size, VkCommandPool cmdPool);

    VkBuffer handle() const {return m_buffer;}
    VkDeviceSize size() const {return m_size;}

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize m_size = 0;
    VkMemoryPropertyFlags m_memoryFlags = 0;
    VkBufferCreateInfo bufferInfo{};
    const Context* m_ctx = nullptr;
};