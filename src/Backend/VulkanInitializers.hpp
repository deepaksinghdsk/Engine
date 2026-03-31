#pragma once 

#include <vulkan/vulkan.h>

namespace vkd
{
    namespace initializers
    {
        inline VkSamplerCreateInfo samplerCreateInfo()
        {
            VkSamplerCreateInfo samplerCreateInfo{};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            return samplerCreateInfo;
        }

        inline VkFramebufferCreateInfo framebufferCreateInfo()
		{
			VkFramebufferCreateInfo framebufferCreateInfo {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			return framebufferCreateInfo;
		}
    }
}