#include "VulkanDevice.h"
#include <assert.h>

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
{
    this->physicalDevice = physicalDevice;

    // Store Properties features, limits and properties of the physical device for later use
    // Device properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    // Queue family properties
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, extensions.data()))
        {
            for (auto &ext : extensions)
            {
                supportedExtensions.push_back(ext.extensionName);
            }
        }
    }
}

VulkanDevice::~VulkanDevice()
{
    vkDestroyDevice(logicalDevice, nullptr);
}

VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, bool useSwapChain)
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    findQueueFamilies(physicalDevice);

    float queuePriority = 1.0f;
    // Just for graphics queue as of now
    {
        VkDeviceQueueCreateInfo graphicsQueue{};
        graphicsQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicsQueue.pQueuePriorities = &queuePriority;
        graphicsQueue.queueFamilyIndex = queueFamilyIndices.graphics.value();
        graphicsQueue.queueCount = 1;
        queueCreateInfos.push_back(graphicsQueue);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    std::vector<const char *> deviceExtensions(enabledExtensions);
    if (useSwapChain)
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for(const char* enabledExtension : deviceExtensions)
    {
        if(std::find(supportedExtensions.begin(), supportedExtensions.end(), enabledExtension) == supportedExtensions.end())
        {
            std::cerr << "Enabled device extension " << enabledExtension << " is not present at device level\n" << std::endl;  
        }
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }
}

void VulkanDevice::findQueueFamilies(VkPhysicalDevice device)
{
    // queueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIndices.graphics = i;
        }

        /*  VkBool32 presentSupport = false;
         vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

         if (presentSupport)
         {
             queueFamilyIndices.present = i;
         } */

        if (queueFamilyIndices.isComplete())
        {
            break;
        }

        i++;
    }

    // return indices;
}
