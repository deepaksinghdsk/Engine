#include "VulkanSwapChain.h"
#include <algorithm>

VulkanSwapChain::~VulkanSwapChain()
{
    // Surface is owned by Context, not VulkanSwapChain
    // Do NOT destroy surface here - it will be destroyed by Context
    cleanup();
}

void VulkanSwapChain::setContext(VkInstance instance, VkDevice device, Context &context)
{
    this->instance = instance;
    this->device = device;
    this->physicalDevice = context.physicalDevice;
    this->surface = context.surface;
    this->context = &context;
}

void VulkanSwapChain::initSurface(GLFWwindow *window)
{
    // if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("failed to create window surface!");
    // }

    // Get list of supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> supportedFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, supportedFormats.data());

    selectedFormat = supportedFormats[0];
    std::vector<VkFormat> prefferedFormats{
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32};

    for (VkSurfaceFormatKHR availableFormat : supportedFormats)
    {
        if (std::find(prefferedFormats.begin(), prefferedFormats.end(), availableFormat.format) != prefferedFormats.end())
        {
            selectedFormat = availableFormat;
        }
    }

    colorFormat = selectedFormat.format;
    colorSpace = selectedFormat.colorSpace;
}

void VulkanSwapChain::create(int width, int height, CommandBuffer *cmdBuffer)
{
    m_cmdBuffer = cmdBuffer;

    VkSurfaceCapabilitiesKHR surfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);

    if (surfCaps.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        swapchainExtent = surfCaps.currentExtent;
    }
    else
    {
        /*int width, height;
        glfwGetFramebufferSize(&window, &width, &height); */

        swapchainExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};

        swapchainExtent.width = std::clamp(swapchainExtent.width, surfCaps.minImageExtent.width, surfCaps.maxImageExtent.width);
        swapchainExtent.height = std::clamp(swapchainExtent.height, surfCaps.minImageExtent.height, surfCaps.maxImageExtent.height);
    }

    uint32_t presentModecount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModecount, nullptr);
    std::vector<VkPresentModeKHR> presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModecount, presentModes.data());

    swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const VkPresentModeKHR &presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    imageCount = surfCaps.minImageCount + 1;
    if (surfCaps.maxImageCount > 0 && imageCount > surfCaps.maxImageCount)
        imageCount = surfCaps.maxImageCount;

    VkSwapchainCreateInfoKHR swapchainCI{};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = imageCount;
    swapchainCI.imageFormat = colorFormat;
    swapchainCI.imageColorSpace = colorSpace;
    swapchainCI.imageExtent = swapchainExtent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.preTransform = surfCaps.currentTransform;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCI.presentMode = swapchainPresentMode;
    swapchainCI.clipped = VK_TRUE;
    // swapchainCI.oldSwapchain;

    if (vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swapchain");

    createImageViews();
    createDepthImage();
}

void VulkanSwapChain::createDepthImage()
{
    // Depth image
    depthImage = new Image(context);
    VkFormat depthFormat = depthImage->findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                           VK_IMAGE_TILING_OPTIMAL,
                                                           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthImage->createImage(swapchainExtent.width,
                            swapchainExtent.width,
                            depthFormat,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImage->transitionImageLayout(m_cmdBuffer->getCmdPool(),
                                      depthFormat,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    depthImage->createImageView(VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanSwapChain::createImageViews()
{
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());

    for(VkImage &img : images)
    {
        imgs.push_back(new Image(context, img));
    }

    for(Image *img : imgs)
    {
        img->createImageView(VK_IMAGE_VIEW_TYPE_2D, colorFormat);
    }

    // Now get the swapchain image buffers containing the image and imageView
    imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        /* VkImageViewCreateInfo colorAttachmentView{};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = NULL;
        colorAttachmentView.flags = 0;
        colorAttachmentView.image = images[i];
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = {

        };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.subresourceRange.levelCount = 1;

        if (vkCreateImageView(device, &colorAttachmentView, nullptr, &imageViews[i]) != VK_SUCCESS)
            std::runtime_error("Failed to create imageView"); */

        imageViews[i] = imgs[i]->getImageView();
    }
}

void VulkanSwapChain::createFrameBuffer(VkRenderPass &renderPass)
{
    swapchainFramebuffers.resize(imageViews.size());

    for (uint32_t i = 0; i < imageCount; i++)
    {
        std::array<VkImageView, 2> attachments =
            {
                imageViews[i],
                depthImage->getImageView()
            };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
}

void VulkanSwapChain::recreateSwapChain(VkRenderPass &renderPass)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(context->window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(context->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanup();
    create(width, height, m_cmdBuffer);
    createImageViews();
    createDepthImage();
    createFrameBuffer(renderPass);
}

void VulkanSwapChain::cleanup()
{
    if (swapChain != VK_NULL_HANDLE)
    {
        for (auto framebuffer : swapchainFramebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);

        for (auto imageView : imageViews)
            vkDestroyImageView(device, imageView, nullptr);

        delete depthImage;

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    swapChain = VK_NULL_HANDLE;
}
