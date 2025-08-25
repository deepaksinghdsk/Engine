#include "VulkanSwapChain.h"

void VulkanSwapChain::setContext(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice)
{
    this->instance = instance;
    this->device = device;
    this->physicalDevice = physicalDevice;
}

void VulkanSwapChain::initSurface(GLFWwindow *window)
{
    if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    // Get list of supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    
    std::vector<VkSurfaceFormatKHR> supportedFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, supportedFormats.data());
    
    VkSurfaceFormatKHR selectedFormat = supportedFormats[0];
    std::vector<VkFormat> prefferedFormats{
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32
    };

    for(VkSurfaceFormatKHR availableFormat:supportedFormats)
    {
        if(std::find(prefferedFormats.begin(), prefferedFormats.end(), availableFormat.format) != prefferedFormats.end())
        {
            selectedFormat = availableFormat;
        }
    }

    colorFormat = selectedFormat.format;
    colorSpace = selectedFormat.colorSpace;
}

void VulkanSwapChain::create(int width, int height)
{
    VkSurfaceCapabilitiesKHR surfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);

    VkExtent2D swapchainExtent;
    if(surfCaps.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        swapchainExtent = surfCaps.currentExtent;
    }else
    {
        /*int width, height;
        glfwGetFramebufferSize(&window, &width, &height); */

        swapchainExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        swapchainExtent.width = std::clamp(swapchainExtent.width, surfCaps.minImageExtent.width, surfCaps.maxImageExtent.width);
        swapchainExtent.height = std::clamp(swapchainExtent.height, surfCaps.minImageExtent.height, surfCaps.maxImageExtent.height);
    }

    uint32_t presentModecount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModecount, nullptr);
    std::vector<VkPresentModeKHR> presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModecount, presentModes.data());

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for(const VkPresentModeKHR& presentMode : presentModes)
    {
        if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    uint32_t imageCount = surfCaps.minImageCount + 1;
    if(surfCaps.maxImageCount > 0 && imageCount > surfCaps.maxImageCount)
        imageCount = surfCaps.maxImageCount;

    VkSwapchainCreateInfoKHR swapchainCI{};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = imageCount;
    swapchainCI.imageFormat = colorFormat;
    swapchainCI.imageColorSpace = colorSpace;
    swapchainCI.imageExtent = swapchainExtent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.preTransform = surfCaps.currentTransform;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCI.presentMode = swapchainPresentMode;
    swapchainCI.clipped = VK_TRUE;
    //swapchainCI.oldSwapchain;

    if(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain)!= VK_SUCCESS)
        throw std::runtime_error("failed to create swapchain");

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());

    // Now get the swapchain image buffers containing the image and imageView
    imageViews.resize(imageCount);
    for(auto i =0; i<imageCount; i++)
    {
        VkImageViewCreateInfo colorAttachmentView{};
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

        if(vkCreateImageView(device, &colorAttachmentView, nullptr, &imageViews[i]) != VK_SUCCESS)
            std::runtime_error("Failed to create imageView");
    }
}

VkResult VulkanSwapChain::aquireNextImage()
{

}

VkResult VulkanSwapChain::queuePresent()
{
    
}

void VulkanSwapChain::cleanup()
{
    if(swapChain != VK_NULL_HANDLE)
    {
        for(auto i = 0; i<imageCount; i++)
            vkDestroyImageView(device, imageViews[i], nullptr);
           
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    
    if(surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, surface, nullptr);

    surface = VK_NULL_HANDLE;
    swapChain = VK_NULL_HANDLE;
}

