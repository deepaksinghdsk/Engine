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

#include "Model.h"

#include "imgui.h"

class Buffer;
class CommandBuffer;
class RenderPass;

class Application
{
private:
    Context context;
    VulkanSwapChain vulkanSwapChain;
    Layer *layer;
    CommandBuffer *cmdBuffer;
    Pipeline *pipeline;
    RenderPass *renderPass;
    // Pipeline pipeline;
    Buffer *vertexBuffer;
    Buffer *indexBuffer;
    DescriptorManager *descManager;
    std::vector<Buffer *> uniformBuffers;
    std::vector<Buffer*> lightUniformBuffers;
    Image *texImage;

    DescriptorManager *imguiDesc;
    // Model *model;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    ImGuiIO *io;

    /*  const std::vector<Vertex> vertices = {
         {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // bottom left - red
         {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},  // bottom right - green
         {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},   // top right - blue
         {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},  // top left - yellow

         {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
         {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
         {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
         {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
     };

     const std::vector<uint16_t> indices = {
         0, 1, 2, // first triangle
         2, 3, 0,  // second triangle

         4, 5, 6,
         6, 7, 4
     }; */

    struct uniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct lightUBO
    {
        glm::vec3 position;
	    glm::vec3 intensities; //a.k.a the color of the light
	    float attenuation;
	    float ambientCoefficient;
    };

public:
    Application(Layer *layer);
    ~Application();

    static Camera *camera;
    void createSyncObjects();
    void run();
    void updateUniformBuffer(uint32_t currentImage, const lightUBO& lightUBO);
    void Draw();

    static void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }
};