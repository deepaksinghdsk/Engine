#include <vulkan/vulkan.h>
#include <vector>
#include <String>
#include <iostream>
#include <optional>

struct VulkanDevice
{
    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;
	/** @brief Properties of the physical device including limits that the application can check against */
	VkPhysicalDeviceProperties properties;
	/** @brief Features of the physical device that an application can use to check if a feature is supported */
	VkPhysicalDeviceFeatures features;
	/** @brief Features that have been enabled for use on the physical device */
	VkPhysicalDeviceFeatures enabledFeatures;
	/** @brief Memory types and heaps of the physical device */
	VkPhysicalDeviceMemoryProperties memoryProperties;
	/** @brief Queue family properties of the physical device */
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	/** @brief List of extensions supported by the device */
	std::vector<std::string> supportedExtensions;
	/** @brief Default command pool for the graphics queue family index */
	VkCommandPool commandPool = VK_NULL_HANDLE;
	/** @brief Contains queue family indices */
	struct
	{
		std::optional<uint32_t> graphics;
        //std::optional<uint32_t> present;
		uint32_t compute;
		uint32_t transfer;

        bool isComplete()
        {
            return graphics.has_value();
        }
	} queueFamilyIndices;
    
    operator VkDevice() const
    {
        return logicalDevice;
    }
    explicit VulkanDevice(VkPhysicalDevice physicalDevice);
    ~VulkanDevice();
    VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, bool useSwapChain = true);
    void findQueueFamilies(VkPhysicalDevice device);
};