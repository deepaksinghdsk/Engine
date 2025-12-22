#pragma once
#include <vulkan/vulkan.h>
#include "Context.h"

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
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags
    );

    void destroy();
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void upload(const void* data, VkDeviceSize size, VkDeviceSize offset);

    VkBuffer handle() const {return m_buffer;}
    VkDeviceSize size() const {return m_size;}

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    const Context* m_ctx = nullptr;
};