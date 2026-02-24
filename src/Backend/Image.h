#pragma once
#include "vulkan/vulkan.h"
#include "string"
#include "Context.h"

class Image
{
public:
    Image(const Context *ctx);
    ~Image();

    // Non-copyable non-movable
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other) noexcept;

    // create image
    void createImage(uint32_t width, uint32_t height,
                     VkImageLayout initialLayout,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     int arrayLayers = 1,
                     VkImageCreateFlags flags = NULL);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createImageView(VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t layerCount = 1);
    void createImageSampler(VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

    void copyBuffer(const VkBuffer &srcBuffer,
                    VkCommandPool cmdPool,
                    const VkBufferImageCopy *region,
                    uint32_t regionCount = 1);
    void transitionImageLayout(VkCommandPool cmdPool,
                               VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t layerCount = 1);
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);

    VkImageView getImageView() { return m_imageView; }
    VkSampler getSampler() { return m_imageSampler; }
    VkFormat getFormat() { return m_format; }

private:
    const Context *m_ctx = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkSampler m_imageSampler = VK_NULL_HANDLE;
    VkFormat m_format;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};