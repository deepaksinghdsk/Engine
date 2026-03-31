#pragma once
#include "Context.h"
#include "Pipeline.h"
#include "Layer.h"
#include "DescriptorManager.h"
#include "Camera.h"
#include "Image.h"
#include "VulkanSwapChain.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

#include "imgui.h"

class Buffer;
class CommandBuffer;
class RenderPass;
class Model;

class Application
{
protected:
    Context context;
    VulkanSwapChain vulkanSwapChain;
    //Application *layer;
    CommandBuffer *cmdBuffer;

    RenderPass *renderPass;

    DescriptorManager *imguiDesc;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    ImGuiIO *io;

public:
    Application();
    //Application(Application *layer);
    ~Application();

    static Camera *camera;
    void createSyncObjects();
    void init();
    void runLoop();
    //void updateUniformBuffer(uint32_t currentImage, const lightUBO& lightUBO);
    uint32_t beginDraw();
    void endDraw(uint32_t imgInd);

    static void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }

    
    virtual void run(uint32_t imgInd) = 0;
    virtual void prepare() = 0;
    virtual void onUIRender() = 0;
};