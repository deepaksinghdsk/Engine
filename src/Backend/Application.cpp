#include "Application.h"
#include "RenderPass.h"
#include "CommandBuffer.h"

std::vector<VkSemaphore> imageAvailableSemaphores;
std::vector<VkSemaphore> renderFinishedSemaphores;
std::vector<VkFence> inFlightFences;

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;


Application::Application(Layer *layer) : layer(layer)
{
    std::cout << "init started" << std::endl
              << std::endl;
    // Physical and logical device is auto created by context class cons
    vulkanSwapChain.setContext(context.instance, context.vulkanDevice->logicalDevice, context);
    vulkanSwapChain.initSurface(context.window);
    // context.setSurface(vulkanSwapChain.surface);
    vulkanSwapChain.create(context.width, context.height);

    renderPass = new RenderPass(context, vulkanSwapChain);
    pipeline = new Pipeline(context.vulkanDevice->logicalDevice, renderPass->renderPass, vulkanSwapChain.swapchainExtent);
    vulkanSwapChain.createFrameBuffer(renderPass->renderPass);

    cmdBuffer = new CommandBuffer(context, MAX_FRAMES_IN_FLIGHT);
    vertexBuffer = new Buffer();
    vertexBuffer->create(context, 
        sizeof(vertices[0])*vertices.size(), 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vertexBuffer->upload(vertices.data(), vertices.size(), 0);
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
    delete vertexBuffer;
    delete pipeline;
    delete renderPass;
    vulkanSwapChain.cleanup();
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
    cmdBuffer->recordCommandBuffer(cmdBuffer->commandBuffers[currentFrame], 
        pipeline->pipeline, 
        renderPass->renderPass, 
        vulkanSwapChain.swapchainFramebuffers[imageIndex], 
        vulkanSwapChain.swapchainExtent,
        vertexBuffers, offsets, vertices.size());

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