#pragma once
#include <vulkan/vulkan.h>
#include "Context.h"

class Buffer;

// Structure to describe a descriptor binding
struct DescriptorBinding
{
    uint32_t binding;
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    const VkSampler *pImmutableSamplers = nullptr;
};

struct DescriptorResource
{
    DescriptorBinding descBinding;

    //one of these is valid depending on type
    VkDescriptorBufferInfo bufferInfo{};
    std::vector<VkDescriptorImageInfo> imageInfos;

    /* static DescriptorResource uniformBuffer(
        VkDescriptorType type,
        VkBuffer buffer,
        VkDeviceSize size)
    {
        DescriptorResource r{};
        r.descriptorType = type;
        r.bufferInfo = {buffer, 0, size};
        return r;
    }

    static DescriptorResource image(
        VkDescriptorType type,
        VkSampler sampler,
        VkImageView imgView,
        VkImageLayout imgLayout)
    {
        DescriptorResource r{};
        r.descriptorType = type;
        r.imageInfo = {sampler, imgView, imgLayout};
        return r;
    } */
};

class DescriptorManager
{
public:
    DescriptorManager(const Context &ctx);
    ~DescriptorManager();

    // Non-copyable, Non-movable
    DescriptorManager(const DescriptorManager &) = delete;
    DescriptorManager &operator=(const DescriptorManager &) = delete;
    DescriptorManager(DescriptorManager &&other) noexcept;
    DescriptorManager &operator=(DescriptorManager &&other) noexcept;

    void createDescriptorSetLayout(const std::vector<DescriptorBinding> &bindings);
    void createDescriptorPool(const int MaxFramesInFlight, const std::vector<DescriptorBinding> &bindings);
    void createDescriptorSets(const int MaxFramesInFlight,
                              const std::vector<std::vector<DescriptorResource>> &perFrameResource);

    std::vector<VkDescriptorSet> handle() const { return descriptorSets; }
    VkDescriptorSetLayout getDescSetLayout() const { return m_descriptorSetLayout; };
    VkDescriptorPool getDescPool() const { return descriptorPool; };

private:
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    const Context *m_ctx = nullptr;
};
