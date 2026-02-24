#include "Application.h"
#include "RenderPass.h"
#include "CommandBuffer.h"
#include "Buffer.h"

#include "Model.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

// Static member initialization
Camera *Application::camera = nullptr;

float deltaTime = 0.0f;
float lastDelta = 0.0f;

bool firstMouse = true;
bool leftHeld = false;
float lastX = 0;
float lastY = 0;

void processInput(GLFWwindow *window);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void mouseButton_Callback(GLFWwindow *window, int button, int action, int mods);

Application::Application()
{
    glfwSetMouseButtonCallback(context.window, mouseButton_Callback);
    glfwSetCursorPosCallback(context.window, mouse_callback);
    glfwSetScrollCallback(context.window, scroll_callback);

    // Physical and logical device is auto created by context class cons
    vulkanSwapChain.setContext(context.instance, context.vulkanDevice->logicalDevice, context);
    vulkanSwapChain.initSurface(context.window);
    // context.setSurface(vulkanSwapChain.surface);

    // Create command buffer early - needed for buffer transfers
    cmdBuffer = new CommandBuffer(context, MAX_FRAMES_IN_FLIGHT);
    vulkanSwapChain.create(context.width, context.height, cmdBuffer);

    renderPass = new RenderPass(context, vulkanSwapChain);
    renderPass->createRenderPass(vulkanSwapChain.getDepthImage()->getFormat());
    vulkanSwapChain.createFrameBuffer(renderPass->renderPass);
}

void Application::init()
{
    prepare();

    lastX = (float)vulkanSwapChain.swapchainExtent.width;
    lastY = (float)vulkanSwapChain.swapchainExtent.height;

    // Setup Dear ImGui context
    {
        // Imgui Descriptor pool
        std::vector<DescriptorBinding> binding = {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE}};
        imguiDesc = new DescriptorManager(context);
        imguiDesc->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, binding);

        // Imgui setup
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        (void)io;
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(context.window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        // init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
        init_info.Instance = context.instance;
        init_info.PhysicalDevice = context.physicalDevice;
        init_info.Device = context.device;
        init_info.QueueFamily = context.vulkanDevice->queueFamilyIndices.graphics.value();
        init_info.Queue = context.graphicsQueue;
        // init_info.PipelineCache = g_PipelineCache;
        init_info.DescriptorPool = imguiDesc->getDescPool();
        init_info.MinImageCount = vulkanSwapChain.imageCount;
        init_info.ImageCount = vulkanSwapChain.imageCount;
        // init_info.Allocator = g_Allocator;
        init_info.PipelineInfoMain.RenderPass = renderPass->renderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = Application::check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);
    }

    createSyncObjects();

    std::cout << "init done successfully" << std::endl;
}

void Application::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(context.vulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context.vulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context.vulkanDevice->logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create semaphores");
    }
}

Application::~Application()
{
    // Wait for device to finish before cleanup
    if (context.device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context.device);
    }

    // Cleanup ImGui BEFORE descriptor manager
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup ImGui descriptor manager
    if (imguiDesc != nullptr)
    {
        delete imguiDesc;
        imguiDesc = nullptr;
    }
}

void Application::runLoop()
{
    while (!glfwWindowShouldClose(context.window))
    {
        glfwPollEvents();

        //Aquire Image and do syncronisation
        uint32_t imgInd = beginDraw();

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        onUIRender();

        ImGui::Begin("Stats");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
        ImGui::End();
        ImGui::Render();

        // Draw the frame
        run(imgInd);

        // End the Draw submit the frame
        endDraw(imgInd);
    }

    vkDeviceWaitIdle(context.device);
}

uint32_t Application::beginDraw()
{
    float currentDelta = static_cast<float>(glfwGetTime());
    deltaTime = currentDelta - lastDelta;
    lastDelta = currentDelta;

    vkWaitForFences(context.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(context.device, 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context.device, vulkanSwapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // Handle out-of-date swapchain by recreating and retrying
    while (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        ImGui_ImplVulkan_SetMinImageCount(vulkanSwapChain.imageCount);
        vulkanSwapChain.recreateSwapChain(renderPass->renderPass);
        result = vkAcquireNextImageKHR(context.device, vulkanSwapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to aquire swap chain image!");

    processInput(context.window);

    return imageIndex;
}

/*
At a high level, rendering a frame in Vulkan consists of a common set of steps:
    Wait for the previous frame to finish
    Acquire an image from the swap chain
    Record a command buffer which draws the scene onto that image
    Submit the recorded command buffer
    Present the swap chain image
*/
void Application::endDraw(uint32_t imgInd)
{
    // submitting the cmd buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer->commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw cmd buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {vulkanSwapChain.swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imgInd;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(context.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || context.framebufferResized)
    {
        context.framebufferResized = false;
        vulkanSwapChain.recreateSwapChain(renderPass->renderPass);
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
        Application::camera->resetCamera();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        Application::camera->ProcessKeyboard(UP, deltaTime);
}

void mouseButton_Callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            leftHeld = true;
            firstMouse = true;
        }
        else if (action == GLFW_RELEASE)
        {
            leftHeld = false;
        }
    }
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    if (leftHeld)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

        lastX = xpos;
        lastY = ypos;

        Application::camera->ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    Application::camera->ProcessMouseScroll(static_cast<float>(yoffset));
}
