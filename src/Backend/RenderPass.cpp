#include "RenderPass.h"

RenderPass::RenderPass(Context &context) : context(context)
{
    //vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(context.device, renderPass, nullptr);
}

RenderPass::~RenderPass()
{
    vkDestroyRenderPass(context.device, renderPass, nullptr);
}

void RenderPass::init()
{
    //color buffer Attachement
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = context.vulkanSwapChain.colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    //References to the Attachments for use in subpasses
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;//index of the Attachment to be refrenced
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    //Index of the attachment in this array is referenced in frag shader using directive
    //layout(location = 0) out vec4 outColor;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if(vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");
}

