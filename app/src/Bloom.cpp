#include "Backend/Application.h"
#include "Backend/Buffer.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#include "Backend/VulkanInitializers.hpp"

Buffer *vertexBuffer;
Buffer *indexBuffer;
Buffer *skyboxVertexBuffer;

Buffer *fullscreenQuadVertexBuffer;

DescriptorManager *modelDescManager;
DescriptorManager *cubeMapDescriptor;
DescriptorManager *glowDescriptor;
DescriptorManager *vertBlurDescriptor;
DescriptorManager *horiBlurDescriptor;
DescriptorManager *compositeDescriptor;

Pipeline *modelPipeline;
Pipeline *cubeMapPipeline;
Pipeline *glowPipeline;
Pipeline *vertBlurPipeline;
Pipeline *horiBlurPipeline;
Pipeline *compositePipeline;

std::vector<Buffer *> uniformBuffers;
std::vector<Buffer *> lightUniformBuffers;
std::unordered_map<uint32_t, int> matIndTexInd;

std::vector<Image *> texImages;
Image *cubeMapImage;

Model *c_model;
Camera *camera = nullptr;

struct uniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct lightUBO
{
    glm::vec3 position;
    glm::vec3 intensities; // the color of the light
    float attenuation;
    float ambientCoefficient;
};

class Bloom : public Application
{
private:
    struct FrameBuffer
    {
    public:
        VkFramebuffer frameBuffer = nullptr;
        Image *color, *depth;
        // As this FB will also be used input for next pass
        // DescriptorManager *descriptor;

        FrameBuffer(Context *ctx)
        {
            color = new Image(ctx);
            depth = new Image(ctx);
            // descriptor = new DescriptorManager(*ctx);
        }

        ~FrameBuffer()
        {
            delete color;
            delete depth;
        }
    };

    struct OffScreenPass
    {
    public:
        int32_t width, height;
        VkRenderPass renderPass = nullptr;
        VkSampler sampler;
        std::vector<FrameBuffer *> frameBuffers{3};

        OffScreenPass(Context *ctx)
        {
            frameBuffers[0] = new FrameBuffer(ctx);
            frameBuffers[1] = new FrameBuffer(ctx);
            frameBuffers[2] = new FrameBuffer(ctx);
        }

        ~OffScreenPass()
        {
            for (uint32_t i = 0; i < 3; i++)
                delete frameBuffers[i];
        }
    };
    OffScreenPass *offScreenPass;
    VkRenderPass imguiRenderPass = nullptr; // Render pass for ImGui composition (loads content, doesn't clear)

    struct fullscreenQuadVertex
    {
        glm::vec3 position;

        static VkVertexInputBindingDescription getBindingDesc()
        {
            VkVertexInputBindingDescription bindingDesc{};
            bindingDesc.binding = 0;
            bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDesc.stride = sizeof(fullscreenQuadVertex);
            return bindingDesc;
        }

        static std::vector<VkVertexInputAttributeDescription> getAttributeDesc()
        {
            std::vector<VkVertexInputAttributeDescription> attribDesc{1};
            attribDesc[0].binding = 0;
            attribDesc[0].location = 0;
            attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribDesc[0].offset = offsetof(fullscreenQuadVertex, position);

            return attribDesc;
        }
    };

    const std::vector<fullscreenQuadVertex> fullscreenQuadVertices =
        {
            {{-1.0f, 1.0f, 0.0f}},  // 1: bottom-left
            {{-1.0f, -1.0f, 0.0f}}, // 0: top-left
            {{1.0f, 1.0f, 0.0f}},   // 2: bottom-right
            {{-1.0f, -1.0f, 0.0f}}, // 3: top-left
            {{1.0f, 1.0f, 0.0f}},   // 4: bottom-right
            {{1.0f, -1.0f, 0.0f}}}; // 5: top-right

    struct drawData
    {
        float lumThreshold = 0.2;
    } glowData;

    struct compositeData
    {
        VkBool32 toneMapping = true;
        VkBool32 gamma = true;
    } compData;

    std::vector<std::string> items = {"Scene", "Glow", "blur", "Bloom"};
    int selected_item = 0;

public:
    Bloom()
    {
        offScreenPass = new OffScreenPass(&context);
        init();
    }

    ~Bloom()
    {
        // Wait for device to finish all operations before cleanup
        vkDeviceWaitIdle(context.device);

        // Delete model first
        delete c_model;

        // Delete buffers
        delete vertexBuffer;
        delete indexBuffer;
        delete skyboxVertexBuffer;
        delete fullscreenQuadVertexBuffer;

        // Delete pipelines
        delete modelPipeline;
        delete cubeMapPipeline;
        delete glowPipeline;
        delete vertBlurPipeline;
        delete horiBlurPipeline;
        delete compositePipeline;

        // Delete RenderPasses
        delete renderPass;
        vkDestroyRenderPass(context.device, offScreenPass->renderPass, nullptr);
        vkDestroyRenderPass(context.device, imguiRenderPass, nullptr);
        vkDestroySampler(context.device, offScreenPass->sampler, nullptr);

        // Delete FrameBuffers
        for (uint16_t i = 0; i < 3; i++)
        {
            vkDestroyFramebuffer(context.device, offScreenPass->frameBuffers[i]->frameBuffer, nullptr);
        }

        delete offScreenPass;

        // Cleanup swapchain
        vulkanSwapChain.cleanup();

        // Delete images
        delete cubeMapImage;
        for (Image *texImage : texImages)
            delete texImage;

        // Delete uniform buffers
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            delete uniformBuffers[i];
            delete lightUniformBuffers[i];
        }

        // Delete descriptor managers
        delete modelDescManager;
        delete cubeMapDescriptor;
        delete glowDescriptor;
        delete vertBlurDescriptor;
        delete horiBlurDescriptor;
        delete compositeDescriptor;

        // Destroy synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(context.device, inFlightFences[i], nullptr);
            vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
        }

        // Delete command buffer
        delete cmdBuffer;
    }

    // For rendering the Scene, then its color attachment will then be sampled from
    void prepareOffScreenFramebuffer(FrameBuffer *frameBuffer)
    {
        // color attachment
        frameBuffer->color = new Image(&context);
        frameBuffer->color->createImage(context.width, context.height,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        frameBuffer->color->createImageView(VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);

        // Create depth attachment
        frameBuffer->depth = new Image(&context);
        frameBuffer->depth->createImage(context.width, context.height,
                                        vulkanSwapChain.getDepthImage()->getFormat(),
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkFormat depthFormat = vulkanSwapChain.getDepthImage()->getFormat();
        frameBuffer->depth->createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                            depthFormat,
                                            VK_IMAGE_ASPECT_DEPTH_BIT | (frameBuffer->depth->hasStencilComponent(depthFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));

        VkImageView imageViews[2];
        imageViews[0] = frameBuffer->color->getImageView();
        imageViews[1] = frameBuffer->depth->getImageView();

        VkFramebufferCreateInfo fbufCreateInfo = vkd::initializers::framebufferCreateInfo();
        fbufCreateInfo.attachmentCount = 2;
        fbufCreateInfo.pAttachments = imageViews;
        fbufCreateInfo.height = context.height;
        fbufCreateInfo.width = context.width;
        fbufCreateInfo.renderPass = offScreenPass->renderPass;
        fbufCreateInfo.layers = 1;

        if (vkCreateFramebuffer(context.device, &fbufCreateInfo, nullptr, &frameBuffer->frameBuffer) != VK_SUCCESS)
            throw std::exception("Unable to create offscreenPass FrameBuffer");
    }

    // Used for the Vert and Hori blur
    void prepareOffScreen()
    {
        offScreenPass->width = context.width;
        offScreenPass->height = context.height;

        // Separate "Render pass" for offscreen oprations
        // color buffer Attachement
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // depth Attachment description
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = vulkanSwapChain.getDepthImage()->getFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // References to the Attachments for use in subpasses
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // index of the Attachment to be refrenced
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        // Index of the attachment in this array is referenced in frag shader using directive
        // layout(location = 0) out vec4 outColor;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        std::array<VkSubpassDependency, 2> dependencies{};

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = 0;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = 0;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &offScreenPass->renderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");

        VkSamplerCreateInfo samplerCI = vkd::initializers::samplerCreateInfo();
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.mipLodBias = 0.0f;
        samplerCI.maxAnisotropy = 1.0f;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = 1.0f;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        if (vkCreateSampler(context.device, &samplerCI, nullptr, &offScreenPass->sampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");

        prepareOffScreenFramebuffer(offScreenPass->frameBuffers[0]);
        prepareOffScreenFramebuffer(offScreenPass->frameBuffers[1]);
        prepareOffScreenFramebuffer(offScreenPass->frameBuffers[2]);
    }

    void preparePipelines()
    {
        // Model Graphics Pipeline
        {
            VkDescriptorSetLayout setLayout = modelDescManager->getDescSetLayout();

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.offset = 0;
            pushRange.size = sizeof(CommandBuffer::DrawData);

            VkPipelineLayout modelPipelineLayout = VK_NULL_HANDLE;
            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &modelPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create modelPipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/vert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/frag.spv";
            pipelineDesc.layout = modelPipelineLayout;
            // pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.renderPass = offScreenPass->renderPass;
            // pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  //WireFrame rendering
            pipelineDesc.vertexBinding = Model::Vertex::getBindingDescription();
            pipelineDesc.vertexAttrib = Model::Vertex::getAttributeDescriptions();

            modelPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            modelPipeline->build(pipelineDesc);
        }

        // CubeMap Graphics Pipeline
        {
            VkDescriptorSetLayout setLayout = cubeMapDescriptor->getDescSetLayout();

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayout cubeMapPipelineLayout = VK_NULL_HANDLE;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &cubeMapPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create cubeMapPipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/cubeMapVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/cubeMapFrag.spv";
            pipelineDesc.layout = cubeMapPipelineLayout;
            pipelineDesc.renderPass = offScreenPass->renderPass;
            // pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.vertexBinding = Model::skyBoxVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = Model::skyBoxVertex::getAttributeDesc();
            pipelineDesc.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL; // Skybox at far plane
            pipelineDesc.depthWrite = false;                         // Don't write to depth buffer

            cubeMapPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            cubeMapPipeline->build(pipelineDesc);
        }

        // Glow pass Graphics pipeline
        {
            VkDescriptorSetLayout setLayout = glowDescriptor->getDescSetLayout();

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.size = sizeof(glowData);
            pushRange.offset = 0;

            // Pipeline layout: to pass on Uniform/Image buffer and push Constant data to shaders
            VkPipelineLayout glowPipelineLayout = VK_NULL_HANDLE;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &glowPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create horiBlurPipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/fullscreenQuadVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/glowFrag.spv";
            pipelineDesc.layout = glowPipelineLayout;
            // pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.renderPass = offScreenPass->renderPass;
            pipelineDesc.vertexBinding = fullscreenQuadVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = fullscreenQuadVertex::getAttributeDesc();
            pipelineDesc.cullMode = VK_CULL_MODE_NONE; // Disable culling for fullscreen quad
            // pipelineDesc.depthTest = false;                  // Disable depth test
            // pipelineDesc.depthWrite = false;                 // Don't write to depth buffer

            glowPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            glowPipeline->build(pipelineDesc);
        }

        // Vertical Blur Graphics pipeline
        {
            VkDescriptorSetLayout setLayout = vertBlurDescriptor->getDescSetLayout();

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayout vertBlurPipelineLayout = VK_NULL_HANDLE;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &vertBlurPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create vertBlurPipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/fullscreenQuadVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/vertBlurFrag.spv";
            pipelineDesc.layout = vertBlurPipelineLayout;
            // pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.renderPass = offScreenPass->renderPass;
            pipelineDesc.vertexBinding = fullscreenQuadVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = fullscreenQuadVertex::getAttributeDesc();
            pipelineDesc.cullMode = VK_CULL_MODE_NONE; // Disable culling for fullscreen quad
            // pipelineDesc.depthTest = false;                  // Disable depth test
            // pipelineDesc.depthWrite = false;                 // Don't write to depth buffer

            vertBlurPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            vertBlurPipeline->build(pipelineDesc);
        }

        // Horizontal Blur Graphics pipeline
        {
            VkDescriptorSetLayout setLayout = horiBlurDescriptor->getDescSetLayout();

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayout horiBlurPipelineLayout = VK_NULL_HANDLE;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &horiBlurPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create BlurPipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/fullscreenQuadVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/horiBlurFrag.spv";
            pipelineDesc.layout = horiBlurPipelineLayout;
            // pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.renderPass = offScreenPass->renderPass;
            pipelineDesc.vertexBinding = fullscreenQuadVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = fullscreenQuadVertex::getAttributeDesc();
            pipelineDesc.cullMode = VK_CULL_MODE_NONE; // Disable culling for fullscreen quad
            // pipelineDesc.depthTest = false;                  // Disable depth test
            // pipelineDesc.depthWrite = false;                 // Don't write to depth buffer

            horiBlurPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            horiBlurPipeline->build(pipelineDesc);
        }

        // Composite pass Graphics pipeline
        {
            VkDescriptorSetLayout setLayout = compositeDescriptor->getDescSetLayout();

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.size = sizeof(compData);
            pushRange.offset = 0;

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayout comositePipelineLayout = VK_NULL_HANDLE;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &comositePipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create compositePipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/fullscreenQuadVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/compositeFrag.spv";
            pipelineDesc.layout = comositePipelineLayout;
            pipelineDesc.renderPass = renderPass->renderPass;
            // pipelineDesc.renderPass = offScreenPass->renderPass;
            pipelineDesc.vertexBinding = fullscreenQuadVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = fullscreenQuadVertex::getAttributeDesc();
            pipelineDesc.cullMode = VK_CULL_MODE_NONE; // Disable culling for fullscreen quad
            // pipelineDesc.depthTest = false;                  // Disable depth test
            // pipelineDesc.depthWrite = false;                 // Don't write to depth buffer

            compositePipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            compositePipeline->build(pipelineDesc);
        }
    }

    void prepareDescriptors()
    {
        // Model Descriptor Sets
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});
            bindings.push_back({1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)texImages.size(), VK_SHADER_STAGE_FRAGMENT_BIT});
            bindings.push_back({2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

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
                for (Image *texImage : texImages)
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = texImage->getImageView();
                    imageInfo.sampler = texImage->getSampler();
                    samplerRes.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes);

                // Light UBO
                DescriptorResource lightUBOres{};
                lightUBOres.descBinding = bindings[2];
                lightUBOres.bufferInfo.buffer = lightUniformBuffers[i]->handle();
                lightUBOres.bufferInfo.offset = 0;
                lightUBOres.bufferInfo.range = sizeof(lightUBO);
                resources[i].push_back(lightUBOres);
            }

            modelDescManager = new DescriptorManager(context);
            modelDescManager->createDescriptorSetLayout(bindings);
            modelDescManager->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            modelDescManager->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }

        // CubeMaps Descriptor set
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});
            bindings.push_back({1,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});

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

                // SkyBox cubeMap
                DescriptorResource samplerRes{};
                samplerRes.descBinding = bindings[1];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = cubeMapImage->getImageView();
                    imageInfo.sampler = cubeMapImage->getSampler();
                    samplerRes.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes);
            }

            cubeMapDescriptor = new DescriptorManager(context);
            cubeMapDescriptor->createDescriptorSetLayout(bindings);
            cubeMapDescriptor->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            cubeMapDescriptor->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }

        // Glow pass Descriptor
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});

            std::vector<std::vector<DescriptorResource>> resources(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                // Scene image sampler
                DescriptorResource samplerRes{};
                samplerRes.descBinding = bindings[0];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = offScreenPass->frameBuffers[0]->color->getImageView(); // getImageView();
                    imageInfo.sampler = offScreenPass->sampler;
                    samplerRes.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes);
            }

            glowDescriptor = new DescriptorManager(context);
            glowDescriptor->createDescriptorSetLayout(bindings);
            glowDescriptor->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            glowDescriptor->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }

        // Vertical Blur Descriptor
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});

            std::vector<std::vector<DescriptorResource>> resources(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                // Scene image sampler
                DescriptorResource samplerRes{};
                samplerRes.descBinding = bindings[0];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = offScreenPass->frameBuffers[1]->color->getImageView(); // getImageView();
                    imageInfo.sampler = offScreenPass->sampler;
                    samplerRes.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes);
            }

            vertBlurDescriptor = new DescriptorManager(context);
            vertBlurDescriptor->createDescriptorSetLayout(bindings);
            vertBlurDescriptor->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            vertBlurDescriptor->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }

        // Horizontal Blur Descriptor
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});

            std::vector<std::vector<DescriptorResource>> resources(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                // Scene image sampler
                DescriptorResource samplerRes{};
                samplerRes.descBinding = bindings[0];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = offScreenPass->frameBuffers[2]->color->getImageView(); // getImageView();
                    imageInfo.sampler = offScreenPass->sampler;
                    samplerRes.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes);
            }

            horiBlurDescriptor = new DescriptorManager(context);
            horiBlurDescriptor->createDescriptorSetLayout(bindings);
            horiBlurDescriptor->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            horiBlurDescriptor->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }

        // composite pass Descriptor
        {
            std::vector<DescriptorBinding> bindings;
            bindings.push_back({0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});
            bindings.push_back({1,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                VK_SHADER_STAGE_FRAGMENT_BIT});

            std::vector<std::vector<DescriptorResource>> resources(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                // Scene image sampler
                DescriptorResource samplerRes0{};
                samplerRes0.descBinding = bindings[0];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = offScreenPass->frameBuffers[0]->color->getImageView(); // getImageView();
                    imageInfo.sampler = offScreenPass->sampler;
                    samplerRes0.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes0);

                // Bloom image sampler
                DescriptorResource samplerRes1{};
                samplerRes1.descBinding = bindings[1];
                {
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = offScreenPass->frameBuffers[1]->color->getImageView(); // getImageView();
                    imageInfo.sampler = offScreenPass->sampler;
                    samplerRes1.imageInfos.push_back(imageInfo);
                }
                resources[i].push_back(samplerRes1);
            }

            compositeDescriptor = new DescriptorManager(context);
            compositeDescriptor->createDescriptorSetLayout(bindings);
            compositeDescriptor->createDescriptorPool(MAX_FRAMES_IN_FLIGHT, bindings);
            compositeDescriptor->createDescriptorSets(MAX_FRAMES_IN_FLIGHT, resources);
        }
    }

    virtual void prepare() override
    {
        camera = new Camera(glm::vec3(0.0f, 0.0f, 7.0f));
        prepareOffScreen();

        // Create ImGui render pass (loads existing content, doesn't clear)
        /* {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = vulkanSwapChain.colorFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // LOAD instead of CLEAR to preserve blit content
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &imguiRenderPass) != VK_SUCCESS)
                throw std::runtime_error("failed to create ImGui render pass!");
        } */

        // 3D ferrari model
        c_model = new Model();

        {
            c_model->loadModel("D:/Dev/Graphics Proj/Engine/res/models/ferrari/scene.obj");
            // indicesSize = c_model->getIndices().size();

            // Vertex Buffer alloc
            {
                Buffer stagingBuffer{};
                stagingBuffer.create(
                    context,
                    (sizeof(c_model->getVertices()[0]) * c_model->getVertices().size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.upload(c_model->getVertices().data(), false);

                vertexBuffer = new Buffer();
                vertexBuffer->create(
                    context,
                    (sizeof(c_model->getVertices()[0]) * c_model->getVertices().size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                vertexBuffer->copyBuffer(stagingBuffer.handle(), (sizeof(c_model->getVertices()[0]) * c_model->getVertices().size()), cmdBuffer->getCmdPool());
            }

            // Index Buffer Alloc
            {
                Buffer stagingBuffer{};
                VkDeviceSize size = (sizeof(c_model->getIndices()[0]) * c_model->getIndices().size());
                stagingBuffer.create(
                    context,
                    size, 0,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.upload(c_model->getIndices().data(), false);

                indexBuffer = new Buffer();
                indexBuffer->create(
                    context,
                    size, 0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                indexBuffer->copyBuffer(stagingBuffer.handle(), size, cmdBuffer->getCmdPool());
            }

            // skybox Vertex Buffer alloc
            {
                Buffer stagingBuffer{};
                stagingBuffer.create(
                    context,
                    (sizeof(c_model->skyBoxVertices[0]) * c_model->skyBoxVertices.size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.upload(c_model->skyBoxVertices.data(), false);

                skyboxVertexBuffer = new Buffer();
                skyboxVertexBuffer->create(
                    context,
                    (sizeof(c_model->skyBoxVertices[0]) * c_model->skyBoxVertices.size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                skyboxVertexBuffer->copyBuffer(stagingBuffer.handle(), (sizeof(c_model->skyBoxVertices[0]) * c_model->skyBoxVertices.size()), cmdBuffer->getCmdPool());
            }

            // fullscreenQuad Vertex Buffer alloc
            {
                Buffer stagingBuffer{};
                stagingBuffer.create(
                    context,
                    (sizeof(fullscreenQuadVertices[0]) * fullscreenQuadVertices.size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.upload(fullscreenQuadVertices.data(), false);

                fullscreenQuadVertexBuffer = new Buffer();
                fullscreenQuadVertexBuffer->create(
                    context,
                    (sizeof(fullscreenQuadVertices[0]) * fullscreenQuadVertices.size()), 0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                fullscreenQuadVertexBuffer->copyBuffer(stagingBuffer.handle(), (sizeof(fullscreenQuadVertices[0]) * fullscreenQuadVertices.size()), cmdBuffer->getCmdPool());
            }
        }

        // MVP Uniform buffer Alloc
        {
            uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                uniformBuffers[i] = new Buffer();
                uniformBuffers[i]->create(context, sizeof(uniformBufferObject), 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            }
        }

        // light uniform buffer Alloc
        {
            lightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                lightUniformBuffers[i] = new Buffer();
                lightUniformBuffers[i]->create(context, sizeof(lightUBO), 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            }
        }

        // Model Texture Image creation
        {
            // loading model textures
            int i = 0;
            for (auto submesh : c_model->getSubmeshes())
            {
                const tinyobj::material_t &material = c_model->getMaterial(submesh.materialIndex);
                int texWidth, texHeight, texChannels;
                stbi_uc *pixels;
                VkFormat imageFormat;

                if (!material.diffuse_texname.empty())
                {
                    std::stringstream ss;
                    ss << "D:/Dev/Graphics Proj/Engine/res/models/ferrari/" << material.diffuse_texname;
                    // pixels = stbi_load("D:/Dev/Graphics Proj/Engine/res/textures/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                    pixels = stbi_load(ss.str().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                    imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
                }
                else
                {
                    // Create a texture with diffuse texColor
                    pixels = new unsigned char[4];
                    pixels[0] = static_cast<unsigned char>(material.diffuse[0] * 255);
                    pixels[1] = static_cast<unsigned char>(material.diffuse[1] * 255);
                    pixels[2] = static_cast<unsigned char>(material.diffuse[2] * 255);
                    pixels[3] = 255; // alpha
                    texWidth = 1;
                    texHeight = 1;
                    imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                }

                VkDeviceSize imageSize = texWidth * texHeight * 4;
                Buffer stagingBuffer{};
                stagingBuffer.create(
                    context,
                    imageSize, 0,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.upload(pixels, false);
                stbi_image_free(pixels);

                Image *texImage = new Image(&context);
                texImage->createImage(texWidth, texHeight,
                                      imageFormat,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                // Purpose: transition the texture iamge to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                // to copy the buffer to image
                texImage->transitionImageLayout(cmdBuffer->getCmdPool(),
                                                imageFormat,
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                // Buffer to Image copy operation
                VkBufferImageCopy region{};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = {0, 0, 0};
                region.imageExtent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
                texImage->copyBuffer(stagingBuffer.handle(),
                                     cmdBuffer->getCmdPool(),
                                     &region);
                // Purpose: transition the texture imAge to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                // to be able to start sampling from the texture image in the shader
                texImage->transitionImageLayout(cmdBuffer->getCmdPool(),
                                                imageFormat,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                texImage->createImageView(VK_IMAGE_VIEW_TYPE_2D, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
                texImage->createImageSampler();
                texImages.push_back(texImage);
                matIndTexInd[submesh.materialIndex] = i;
                i++;
            }
        }

        // CubeMap texture loading
        {
            int faces = 6;
            std::vector<std::string> skyboxTextures{
                "right.jpg",  // +X
                "left.jpg",   // -X
                "top.jpg",    // +Y
                "bottom.jpg", // -Y
                "front.jpg",  // +Z
                "back.jpg"    // -Z
            };

            int texWidth, texHeight, texChannels;
            stbi_uc *pixels[6];
            for (int i = 0; i < faces; i++)
            {
                std::stringstream ss;
                ss << "D:/Dev/Graphics Proj/Engine/res/textures/skybox/" << skyboxTextures[i];
                std::cout << ss.str().c_str() << std::endl;
                pixels[i] = stbi_load(ss.str().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                if (!pixels[i])
                    std::runtime_error("failed to load cubemap image");
            }

            int faceSize = texWidth * texHeight * 4;
            VkDeviceSize imageSize = faceSize * faces;
            Buffer stagingBuffer{};
            stagingBuffer.create(
                context,
                imageSize, 0,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            for (uint32_t face = 0; face < 6; face++)
            {
                stagingBuffer.upload(pixels[face], true, face, faceSize);
            }
            for (const auto &pix : pixels)
                stbi_image_free(pix);

            cubeMapImage = new Image(&context);
            cubeMapImage->createImage(texWidth, texHeight,
                                      VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                      6,
                                      VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
            cubeMapImage->transitionImageLayout(cmdBuffer->getCmdPool(),
                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                6);
            std::vector<VkBufferImageCopy> copyRegions;
            for (int face = 0; face < faces; face++)
            {
                VkBufferImageCopy copyRegion;
                copyRegion.bufferOffset = face * faceSize;
                copyRegion.bufferRowLength = 0; // tightly packed
                copyRegion.bufferImageHeight = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = face;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageOffset = {0, 0, 0};
                copyRegion.imageExtent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
                copyRegions.push_back(copyRegion);
            }
            cubeMapImage->copyBuffer(stagingBuffer.handle(),
                                     cmdBuffer->getCmdPool(),
                                     copyRegions.data(), static_cast<uint32_t>(copyRegions.size()));
            cubeMapImage->transitionImageLayout(cmdBuffer->getCmdPool(),
                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                6);
            cubeMapImage->createImageView(VK_IMAGE_VIEW_TYPE_CUBE,
                                          VK_FORMAT_R8G8B8A8_UNORM,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          6);
            cubeMapImage->createImageSampler();
        }

        prepareDescriptors();

        preparePipelines();
    }

    virtual void onUIRender() override
    {
        static ImVec4 clear_color = ImVec4(0.35f, 0.25f, 0.30f, 1.00f);

        static float f = 0.0f;
        static int counter = 0;
        static glm::vec3 lightPos = glm::vec3(7.0f, 3.0f, 8.0f);
        static glm::vec3 intensities = glm::vec3(1.0f, 1.0f, 1.0f);
        static float atten = 1;
        static float ambCoe = 1;

        ImGui::Begin("Rendering options"); // Create a window called "Hello, world!" and append into it.

        ImGui::SliderFloat("Luminance", &glowData.lumThreshold, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float *)&clear_color);             // Edit 3 floats representing a color

        ImGui::BeginGroup();
        ImGui::Checkbox("ToneMapping", (bool *)&compData.toneMapping);
        ImGui::SameLine();
        ImGui::Checkbox("Gamma", (bool *)&compData.gamma);
        ImGui::EndGroup();

        if (ImGui::BeginCombo("Render Target", items[selected_item].c_str()))
        {
            for (uint32_t i = 0; i < items.size(); i++)
            {
                const bool is_selected = (selected_item == i);

                if (ImGui::Selectable(items[i].c_str(), is_selected))
                    selected_item = i;

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::BeginGroup();
        ImGui::Text("Light Data");
        ImGui::SliderFloat3("light Position", (float *)&lightPos, -15.0f, 15.0f);
        ImGui::ColorEdit3("color", (float *)&intensities);
        ImGui::SliderFloat("attenuation", (float *)&atten, -10.0f, 10.0f);
        ImGui::SliderFloat("AmbientCoefficient", (float *)&ambCoe, -10.0f, 10.0f);
        ImGui::EndGroup();

        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::Text("counter = %d", counter);

        ImGui::End();

        lightUBO ubo{};
        ubo.position = lightPos;
        ubo.intensities = intensities;
        ubo.attenuation = atten;
        ubo.ambientCoefficient = ambCoe;
        updateUniformBuffer(ubo);
    }

    void bitBlit(VkImage blitImage, int imgInd)
    {
        // Barrier 1: Transition offscreen from SHADER_READ_ONLY to TRANSFER_SRC
        VkImageMemoryBarrier srcBarrier{};
        srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        srcBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        srcBarrier.image = blitImage;
        srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        srcBarrier.subresourceRange.baseMipLevel = 0;
        srcBarrier.subresourceRange.levelCount = 1;
        srcBarrier.subresourceRange.baseArrayLayer = 0;
        srcBarrier.subresourceRange.layerCount = 1;
        srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer->commandBuffers[currentFrame],
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &srcBarrier);

        // Barrier 2: Transition swapchain from UNDEFINED to TRANSFER_DST
        VkImageMemoryBarrier dstBarrier{};
        dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dstBarrier.image = vulkanSwapChain.getSwapChainImages()[imgInd]->getImage();
        dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        dstBarrier.subresourceRange.baseMipLevel = 0;
        dstBarrier.subresourceRange.levelCount = 1;
        dstBarrier.subresourceRange.baseArrayLayer = 0;
        dstBarrier.subresourceRange.layerCount = 1;
        dstBarrier.srcAccessMask = 0;
        dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmdBuffer->commandBuffers[currentFrame],
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

        // Setup blit region - copy full extent
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = {0, 0, 0};
        region.srcOffsets[1] = {(int32_t)vulkanSwapChain.swapchainExtent.width,
                                (int32_t)vulkanSwapChain.swapchainExtent.height, 1};

        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = {0, 0, 0};
        region.dstOffsets[1] = {(int32_t)vulkanSwapChain.swapchainExtent.width,
                                (int32_t)vulkanSwapChain.swapchainExtent.height, 1};

        // Blit offscreen to swapchain
        vkCmdBlitImage(cmdBuffer->commandBuffers[currentFrame],
                       blitImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       vulkanSwapChain.getSwapChainImages()[imgInd]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region, VK_FILTER_LINEAR);

        // Barrier 3: Transition swapchain from TRANSFER_DST to PRESENT_SRC
        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.image = vulkanSwapChain.getSwapChainImages()[imgInd]->getImage();
        presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentBarrier.subresourceRange.baseMipLevel = 0;
        presentBarrier.subresourceRange.levelCount = 1;
        presentBarrier.subresourceRange.baseArrayLayer = 0;
        presentBarrier.subresourceRange.layerCount = 1;
        presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentBarrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(cmdBuffer->commandBuffers[currentFrame],
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &presentBarrier);
    }

    virtual void run(uint32_t imgInd) override
    {
        /* cmdBuffer->beginCmd(currentFrame, renderPass->renderPass, vulkanSwapChain.swapchainFramebuffers[imgInd],
                            vulkanSwapChain.swapchainExtent,
                            {1.0f, 1.0f, 1.0f, 1.00f}); */
        cmdBuffer->beginCmdbuffer(currentFrame);

        VkBuffer vertexBuffers[] = {vertexBuffer->handle()};
        VkBuffer skyboxVertexBuffers[] = {skyboxVertexBuffer->handle()};
        VkBuffer fullscreenQuadVertexBuffers[] = {fullscreenQuadVertexBuffer->handle()};
        VkDeviceSize offsets[] = {0};

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(vulkanSwapChain.swapchainExtent.width);
        viewport.height = static_cast<float>(vulkanSwapChain.swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer->commandBuffers[currentFrame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vulkanSwapChain.swapchainExtent;
        vkCmdSetScissor(cmdBuffer->commandBuffers[currentFrame], 0, 1, &scissor);

        struct DrawData
        {
            uint32_t texIndex;
        };

        // 1st Render Pass to render the scene
        cmdBuffer->beginRenderPass(currentFrame, offScreenPass->renderPass,
                                   offScreenPass->frameBuffers[0]->frameBuffer,
                                   vulkanSwapChain.swapchainExtent,
                                   {1.0f, 1.0f, 1.0f, 1.0f});
        {
            // Model pbr rendering
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(cmdBuffer->commandBuffers[currentFrame], indexBuffer->handle(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        modelPipeline->getPipelineLayout(), 0, 1, &modelDescManager->handle()[currentFrame], 0, nullptr);
                for (const Model::Submesh &sm : c_model->getSubmeshes())
                {
                    DrawData data{};
                    data.texIndex = matIndTexInd.at(sm.materialIndex);
                    vkCmdPushConstants(cmdBuffer->commandBuffers[currentFrame], modelPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawData), &data);
                    vkCmdDrawIndexed(cmdBuffer->commandBuffers[currentFrame], sm.indexCount, 1, sm.firstIndex, 0, 0);
                }
            }

            // skybox rendering - render LAST so it doesn't render unnecessary parts of the scene
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, cubeMapPipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, skyboxVertexBuffers, offsets);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        cubeMapPipeline->getPipelineLayout(), 0, 1, &cubeMapDescriptor->handle()[currentFrame], 0, nullptr);
                vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(c_model->skyBoxVertices.size()), 1, 0, 0);
            }
        }
        cmdBuffer->endRenderPass(currentFrame);

        // 2nd Render Pass for GlowPass
        if (selected_item != 0)
        {
            cmdBuffer->beginRenderPass(currentFrame, offScreenPass->renderPass,
                                       offScreenPass->frameBuffers[1]->frameBuffer,
                                       vulkanSwapChain.swapchainExtent,
                                       {1.0f, 1.0f, 1.0f, 1.0f});
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, glowPipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, fullscreenQuadVertexBuffers, offsets);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        glowPipeline->getPipelineLayout(), 0, 1, &glowDescriptor->handle()[currentFrame], 0, nullptr);
                vkCmdPushConstants(cmdBuffer->commandBuffers[currentFrame], glowPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glowData), &glowData);

                vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(fullscreenQuadVertices.size()), 1, 0, 0);
            }

            cmdBuffer->endRenderPass(currentFrame);
        }

        // Third Render Pass for VerticalBlur
        if (selected_item != 0 && selected_item != 1)
        {
            cmdBuffer->beginRenderPass(currentFrame, offScreenPass->renderPass,
                                       offScreenPass->frameBuffers[2]->frameBuffer, // vulkanSwapChain.swapchainFramebuffers[imgInd],
                                       vulkanSwapChain.swapchainExtent,
                                       {1.0f, 1.0f, 1.0f, 1.0f});
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, vertBlurPipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, fullscreenQuadVertexBuffers, offsets);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vertBlurPipeline->getPipelineLayout(), 0, 1, &vertBlurDescriptor->handle()[currentFrame], 0, nullptr);
                vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(fullscreenQuadVertices.size()), 1, 0, 0);
            }
            cmdBuffer->endRenderPass(currentFrame);
        }

        // Fourth Render Pass for HorizontalBlur
        if (selected_item != 0 && selected_item != 1)
        {
            cmdBuffer->beginRenderPass(currentFrame, offScreenPass->renderPass,
                                       offScreenPass->frameBuffers[1]->frameBuffer,
                                       vulkanSwapChain.swapchainExtent,
                                       {1.0f, 1.0f, 1.0f, 1.0f});
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, horiBlurPipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, fullscreenQuadVertexBuffers, offsets);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        horiBlurPipeline->getPipelineLayout(), 0, 1, &horiBlurDescriptor->handle()[currentFrame], 0, nullptr);
                vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(fullscreenQuadVertices.size()), 1, 0, 0);
            }
            cmdBuffer->endRenderPass(currentFrame);
        }

        // Fiveth Render Pass for compositePass
        if (selected_item != 0 && selected_item != 1 && selected_item != 2)
        {
            cmdBuffer->beginRenderPass(currentFrame, renderPass->renderPass,
                                       vulkanSwapChain.swapchainFramebuffers[imgInd],
                                       vulkanSwapChain.swapchainExtent,
                                       {1.0f, 1.0f, 1.0f, 1.0f});
            {
                vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline->pipeline);
                vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, fullscreenQuadVertexBuffers, offsets);
                vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        compositePipeline->getPipelineLayout(), 0, 1, &compositeDescriptor->handle()[currentFrame], 0, nullptr);
                vkCmdPushConstants(cmdBuffer->commandBuffers[currentFrame], compositePipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(compData), &compData);
                vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(fullscreenQuadVertices.size()), 1, 0, 0);
            }
            cmdBuffer->endRenderPass(currentFrame);
        }

        // Bit Blitting with swapchain Image to Present on screen
        if (selected_item == 0)
            bitBlit(offScreenPass->frameBuffers[0]->color->getImage(), imgInd);
        else if (selected_item == 1 || selected_item == 2)
            bitBlit(offScreenPass->frameBuffers[1]->color->getImage(), imgInd);

        // ImGui Render - use render pass that loads (preserves) existing content
        cmdBuffer->beginRenderPass(currentFrame, renderPass->renderPass,
                                   vulkanSwapChain.swapchainFramebuffers[imgInd],
                                   vulkanSwapChain.swapchainExtent,
                                   {0.0f, 0.0f, 0.0f, 0.0f}); // Clear color ignored (LOAD op used)
        ImGui_ImplVulkan_RenderDrawData(
            ImGui::GetDrawData(),
            cmdBuffer->commandBuffers[currentFrame]);
        cmdBuffer->endRenderPass(currentFrame);
        cmdBuffer->endCmdbuffer(currentFrame);
    }

    void updateUniformBuffer(const lightUBO &lightUBO)
    {
        // FPS calc
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        uniformBufferObject ubo{};
        // Ferrari model is extremely small (0.02 units), scale it up by 150x to match normal scale
        ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(150.0f));
        ubo.view = camera->GetViewMatrix();
        ubo.proj = glm::perspective(glm::radians(camera->Zoom), (float)vulkanSwapChain.swapchainExtent.width / (float)vulkanSwapChain.swapchainExtent.height, 0.1f, 100.f);
        ubo.proj[1][1] *= -1;

        // Updating the Buffer data
        uniformBuffers[currentFrame]->upload(&ubo, true);
        lightUniformBuffers[currentFrame]->upload(&lightUBO, true);
    }
};

void main(int argc, char **argv)
{
    Bloom layer{};
    layer.runLoop();
}