#include "Application.h"
#include "RenderPass.h"
#include "CommandBuffer.h"
#include "Buffer.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

// Static member initialization
Camera *Application::camera = nullptr;

float deltaTime = 0.0f;
float lastDelta = 0.0f;
bool firstMouse = true;
float lastX = 0;
float lastY = 0;

uint32_t indicesSize = 0; 

void processInput(GLFWwindow *window);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);

Application::Application(Layer *layer) : layer(layer)
{
    glfwSetCursorPosCallback(context.window, mouse_callback);
    glfwSetScrollCallback(context.window, scroll_callback);

    std::cout << "init started" << std::endl
              << std::endl;

    // Physical and logical device is auto created by context class cons
    vulkanSwapChain.setContext(context.instance, context.vulkanDevice->logicalDevice, context);
    vulkanSwapChain.initSurface(context.window);
    // context.setSurface(vulkanSwapChain.surface);

    // Create command buffer early - needed for buffer transfers
    cmdBuffer = new CommandBuffer(context, MAX_FRAMES_IN_FLIGHT);
    vulkanSwapChain.create(context.width, context.height, cmdBuffer);

    lastX = vulkanSwapChain.swapchainExtent.width;
    lastY = vulkanSwapChain.swapchainExtent.height;
    camera = new Camera(glm::vec3(0.0f, 0.0f, 3.0f));

    // 3D viking model
    {

        Model *model = new Model();
        model->loadModel("D:/Dev/Graphics Proj/Engine/res/models/viking_room.obj");
        indicesSize = model->getIndices().size();

        // Vertex Buffer alloc
        {
            Buffer stagingBuffer{};
            stagingBuffer.create(
                context,
                (sizeof(model->getVertices()[0]) * model->getVertices().size()), 0,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            stagingBuffer.upload(model->getVertices().data(), false);

            vertexBuffer = new Buffer();
            vertexBuffer->create(
                context,
                (sizeof(model->getVertices()[0]) * model->getVertices().size()), 0,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vertexBuffer->copyBuffer(stagingBuffer.handle(), (sizeof(model->getVertices()[0]) * model->getVertices().size()), cmdBuffer->getCmdPool());
        }

        // Index Buffer Alloc
        {
            Buffer stagingBuffer{};
            VkDeviceSize size = (sizeof(model->getIndices()[0]) * model->getIndices().size());
            stagingBuffer.create(
                context,
                size, 0,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            stagingBuffer.upload(model->getIndices().data(), false);

            indexBuffer = new Buffer();
            indexBuffer->create(
                context,
                size, 0,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            indexBuffer->copyBuffer(stagingBuffer.handle(), size, cmdBuffer->getCmdPool());
        }
    }

    // Uniform buffer Alloc
    {
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            uniformBuffers[i] = new Buffer();
            uniformBuffers[i]->create(context, sizeof(uniformBufferObject), 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    // Texture Image creation
    {
        // loading texture
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load("D:/Dev/Graphics Proj/Engine/res/textures/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        Buffer stagingBuffer{};
        stagingBuffer.create(
            context,
            imageSize, 0,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.upload(pixels, false);
        stbi_image_free(pixels);

        texImage = new Image(&context);
        texImage->createImage(texWidth, texHeight,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        texImage->copyBuffer(stagingBuffer.handle(),
                             imageSize,
                             cmdBuffer->getCmdPool(),
                             VK_FORMAT_R8G8B8A8_SRGB,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        texImage->createImageView(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
        texImage->createImageSampler();
    }

    // DescriptorSets creation
    {
        std::vector<DescriptorBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}};

        std::vector<std::vector<DescriptorResource>> resources(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            // uniform buffer
            DescriptorResource bufferRes{};
            bufferRes.descBinding = bindings[0];
            bufferRes.bufferInfo.buffer = uniformBuffers[i]->handle();
            bufferRes.bufferInfo.offset = 0;
            bufferRes.bufferInfo.range = sizeof(uniformBufferObject);
            resources[i].push_back(bufferRes);

            // combined image sampler
            DescriptorResource samplerRes{};
            samplerRes.descBinding = bindings[1];
            samplerRes.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            samplerRes.imageInfo.imageView = texImage->getImageView();
            samplerRes.imageInfo.sampler = texImage->getSampler();
            resources[i].push_back(samplerRes);
        }

        descManager = new DescriptorManager();
        descManager->createDescriptorSetLayout(context, bindings);
        descManager->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
        descManager->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
    }

    renderPass = new RenderPass(context, vulkanSwapChain);
    renderPass->createRenderPass(vulkanSwapChain.getDepthImage()->getFormat());
    pipeline = new Pipeline(context.vulkanDevice->logicalDevice);
    pipeline->initPipeline(descManager->getDescSetLayout(), &renderPass->renderPass, vulkanSwapChain.swapchainExtent);
    vulkanSwapChain.createFrameBuffer(renderPass->renderPass);

    createSyncObjects();
    std::cout << "init done successfully" << std::endl;

    run();
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
    //delete model;
    delete vertexBuffer;
    delete pipeline;
    delete renderPass;
    vulkanSwapChain.cleanup();
    delete texImage;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        delete uniformBuffers[i];
    delete descManager;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(context.vulkanDevice->logicalDevice, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(context.vulkanDevice->logicalDevice, inFlightFences[i], nullptr);
        vkDestroySemaphore(context.vulkanDevice->logicalDevice, imageAvailableSemaphores[i], nullptr);
    }

    delete cmdBuffer;
}

void Application::run()
{
    while (!glfwWindowShouldClose(context.window))
    {
        glfwPollEvents();
        layer->run();

        // Draw the frame
        Draw();
    }

    vkDeviceWaitIdle(context.device);
}

void Application::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    uniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f); // glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    // ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = camera->GetViewMatrix();
    // ubo.proj = glm::perspective(glm::radians(45.0f), vulkanSwapChain.swapchainExtent.width / (float)vulkanSwapChain.swapchainExtent.height, 0.1f, 10.f);
    ubo.proj = glm::perspective(glm::radians(camera->Zoom), (float)vulkanSwapChain.swapchainExtent.width / (float)vulkanSwapChain.swapchainExtent.height, 0.1f, 10.f);
    ubo.proj[1][1] *= -1;

    uniformBuffers[currentFrame]->upload(&ubo, true);
}

/*
At a high level, rendering a frame in Vulkan consists of a common set of steps:
    Wait for the previous frame to finish
    Acquire an image from the swap chain
    Record a command buffer which draws the scene onto that image
    Submit the recorded command buffer
    Present the swap chain image
*/
void Application::Draw()
{
    float currentDelta = static_cast<float>(glfwGetTime());
    deltaTime = currentDelta - lastDelta;
    lastDelta = currentDelta;

    vkWaitForFences(context.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context.device, vulkanSwapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        vulkanSwapChain.recreateSwapChain(renderPass->renderPass);
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to aquire swap chain image!");

    vkResetFences(context.device, 1, &inFlightFences[currentFrame]);
    VkBuffer vertexBuffers[] = {vertexBuffer->handle()};
    VkDeviceSize offsets[] = {0};
    cmdBuffer->recordCommandBuffer(
        cmdBuffer->commandBuffers[currentFrame],
        pipeline->pipeline,
        renderPass->renderPass,
        vulkanSwapChain.swapchainFramebuffers[imageIndex],
        vulkanSwapChain.swapchainExtent,
        vertexBuffers, offsets, indexBuffer->handle(), indicesSize,//model->getIndices().size(),
        descManager->handle()[currentFrame], pipeline->getPipelineLayout());

    processInput(context.window);
    updateUniformBuffer(currentFrame);

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
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(context.presentQueue, &presentInfo);
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

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
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

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    Application::camera->ProcessMouseScroll(static_cast<float>(yoffset));
}
