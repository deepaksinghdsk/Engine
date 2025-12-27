#include "DescriptorManager.h"
#include "Buffer.h"

DescriptorManager::DescriptorManager(const Context& ctx) : m_ctx(&ctx)
{
}

DescriptorManager::~DescriptorManager()
{
    vkDestroyDescriptorPool(m_ctx->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_ctx->device, m_descriptorSetLayout, nullptr);
}

void DescriptorManager::createDescriptorSetLayout(const std::vector<DescriptorBinding> &bindings)
{
    // Convert DescriptorBinding to VkDescriptorSetLayoutBinding
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    for (const auto &binding : bindings)
    {
        VkDescriptorSetLayoutBinding vkLayoutBinding{};
        vkLayoutBinding.binding = binding.binding;
        vkLayoutBinding.descriptorType = binding.descriptorType;
        vkLayoutBinding.descriptorCount = binding.descriptorCount;
        vkLayoutBinding.stageFlags = binding.stageFlags;
        vkLayoutBinding.pImmutableSamplers = binding.pImmutableSamplers; // for image smapling related descriptors
        vkBindings.push_back(vkLayoutBinding);
    }

    // All of the descriptor bindings are combined into a single VkDescriptorSetLayout
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    layoutInfo.pBindings = vkBindings.data();

    if (vkCreateDescriptorSetLayout(m_ctx->device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor set layour!");
}

void DescriptorManager::createDescriptorPool(const int MaxFramesInFlight, const std::vector<DescriptorBinding> &bindings)
{
    // Create pool sizes for each descriptor type
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto &binding : bindings)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = binding.descriptorType;
        poolSize.descriptorCount = binding.descriptorCount * static_cast<uint32_t>(MaxFramesInFlight);
        poolSizes.push_back(poolSize);
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    for (VkDescriptorPoolSize &pool_size : poolSizes)
        poolInfo.maxSets += pool_size.descriptorCount;
    //poolInfo.maxSets = static_cast<uint32_t>(MaxFramesInFlight);

    if (vkCreateDescriptorPool(m_ctx->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");
}

void DescriptorManager::createDescriptorSets(const int MaxFramesInFlight, const std::vector<std::vector<DescriptorResource>> &perFrameResource)
{
    std::vector<VkDescriptorSetLayout> layouts(MaxFramesInFlight, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MaxFramesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MaxFramesInFlight);
    if (vkAllocateDescriptorSets(m_ctx->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocated descriptor sets!");

    // Update each descriptor set
    for (size_t frameId = 0; frameId < MaxFramesInFlight; ++frameId)
    {
        std::vector<VkWriteDescriptorSet> descriptorWrites;

        // for each binding, create a write descriptor set
        for (const auto &res : perFrameResource[frameId])
        {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[frameId];
            descriptorWrite.dstBinding = res.descBinding.binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = res.descBinding.descriptorType;
            descriptorWrite.descriptorCount = res.descBinding.descriptorCount;

            // Only set buffer info if this is a buffer type
            if (res.descBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                res.descBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            {
                descriptorWrite.pBufferInfo = &res.bufferInfo;
            }
            else if (res.descBinding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
                descriptorWrite.pImageInfo = &res.imageInfo;
            }

            descriptorWrites.push_back(descriptorWrite);
        }

        vkUpdateDescriptorSets(m_ctx->device, static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}
