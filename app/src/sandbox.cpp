#include "Backend/Application.h"
#include "Backend/Buffer.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

Buffer *vertexBuffer;
Buffer *indexBuffer;
Buffer *skyboxVertexBuffer;

DescriptorManager *modelDescManager;
DescriptorManager *cubeMapDescriptor;

Pipeline *pipeline;
Pipeline *cubeMapPipeline;

VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkPipelineLayout cubeMapPipelineLayout = VK_NULL_HANDLE;

std::vector<Buffer *> uniformBuffers;
std::vector<Buffer *> lightUniformBuffers;
std::unordered_map<uint32_t, int> matIndTexInd;

std::vector<Image *> texImages;
Image *cubeMapImage;

Model *c_model;

size_t indicesSize = 0;

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

// PushConstant
struct DrawData
{
    uint32_t texIndex;
};

class Example : public Application
{
private:
    VkBool32 glowData = false;
    
public:
    Example()
    {
        init();
    }

    ~Example()
    {
        // Wait for device to finish all operations before cleanup
        vkDeviceWaitIdle(context.device);

        // Delete model first
        delete c_model;

        // Delete buffers
        delete vertexBuffer;
        delete indexBuffer;
        delete skyboxVertexBuffer;

        // Delete pipelines
        delete pipeline;
        delete cubeMapPipeline;
        delete renderPass;

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

        // Destroy pipeline layouts
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        if (cubeMapPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(context.device, cubeMapPipelineLayout, nullptr);

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

    virtual void prepare() override
    {
        camera = new Camera(glm::vec3(0.0f, 0.0f, 7.0f));

        // 3D viking model
        c_model = new Model();
        {
            c_model->loadModel("D:/Dev/Graphics Proj/Engine/res/models/ferrari/scene.obj");
            indicesSize = c_model->getIndices().size();

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

        // light uniform buffer Alloc
        {
            lightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                lightUniformBuffers[i] = new Buffer();
                lightUniformBuffers[i]->create(context, sizeof(lightUBO), 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            }
        }

        // Texture Image creation
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
                                     copyRegions.data(), copyRegions.size());
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

        // Model DescriptorSets creation
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

        // Model Graphics Pipeline
        {
            VkDescriptorSetLayout setLayout = modelDescManager->getDescSetLayout();

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.offset = 0;
            pushRange.size = sizeof(DrawData);

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create pipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/vert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/frag.spv";
            pipelineDesc.layout = pipelineLayout;
            pipelineDesc.renderPass = renderPass->renderPass;
            //pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            pipelineDesc.vertexBinding = Model::Vertex::getBindingDescription();
            pipelineDesc.vertexAttrib = Model::Vertex::getAttributeDescriptions();

            pipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            pipeline->build(pipelineDesc);
            // pipeline->initPipeline(descManager->getDescSetLayout(), &renderPass->renderPass, vulkanSwapChain.swapchainExtent);
        }

        // CubeMap Graphics Pipeline
        {
            VkDescriptorSetLayout setLayout = cubeMapDescriptor->getDescSetLayout();

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.offset = 0;
            pushRange.size = sizeof(glowData);

            // Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &cubeMapPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create pipeline layout!");
            }

            PipelineDesc pipelineDesc{};
            pipelineDesc.vertShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/cubeMapVert.spv";
            pipelineDesc.fragShaderLoc = "D:/Dev/Graphics Proj/Engine/res/shaders/cubeMapFrag.spv";
            pipelineDesc.layout = cubeMapPipelineLayout;
            pipelineDesc.renderPass = renderPass->renderPass;
            pipelineDesc.vertexBinding = Model::skyBoxVertex::getBindingDesc();
            pipelineDesc.vertexAttrib = Model::skyBoxVertex::getAttributeDesc();
            pipelineDesc.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL; // Skybox at far plane
            pipelineDesc.depthWrite = false;                         // Don't write to depth buffer

            cubeMapPipeline = new Pipeline(context.vulkanDevice->logicalDevice);
            cubeMapPipeline->build(pipelineDesc);
        }
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

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color
        ImGui::Checkbox("Glow", (bool *) &glowData);

        ImGui::BeginGroup();
        ImGui::Text("Light Data");
        ImGui::SliderFloat3("light Position", (float *)&lightPos, -15.0f, 15.0f);
        ImGui::ColorEdit3("color", (float *)&intensities);
        ImGui::SliderFloat("attenuation", (float *)&atten, -10.0f, 10.0f);
        ImGui::SliderFloat("AmbientCoefficient", (float *)&ambCoe, -10.0f, 10.0f);
        ImGui::EndGroup();

        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();

        lightUBO ubo{};
        ubo.position = lightPos;
        ubo.intensities = intensities;
        ubo.attenuation = atten;
        ubo.ambientCoefficient = ambCoe;
        updateUniformBuffer(ubo);
    }

    virtual void run(uint32_t imgInd) override
    {
        cmdBuffer->beginCmd(currentFrame, renderPass->renderPass, vulkanSwapChain.swapchainFramebuffers[imgInd],
                            vulkanSwapChain.swapchainExtent,
                            {1.0f, 1.0f, 1.0f, 1.00f});

        VkBuffer vertexBuffers[] = {vertexBuffer->handle()};
        VkBuffer skyboxVertexBuffers[] = {skyboxVertexBuffer->handle()};
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

        // Model pbr rendering
        {
            vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
            vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmdBuffer->commandBuffers[currentFrame], indexBuffer->handle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(), 0, 1, &modelDescManager->handle()[currentFrame], 0, nullptr);
            for (const Model::Submesh &sm : c_model->getSubmeshes())
            {
                DrawData data{};
                data.texIndex = matIndTexInd.at(sm.materialIndex);

                vkCmdPushConstants(cmdBuffer->commandBuffers[currentFrame], pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawData), &data);
                // vkCmdDraw(commandBuffer, static_cast<uint32_t>(vbSize), 1, 0, 0);
                // vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(ibSize), 1, 0, 0, 0);
                vkCmdDrawIndexed(cmdBuffer->commandBuffers[currentFrame], sm.indexCount, 1, sm.firstIndex, 0, 0);
            }
        }

        // skybox rendering - render LAST so it appears behind everything
        {
            vkCmdBindPipeline(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, cubeMapPipeline->pipeline);
            vkCmdBindVertexBuffers(cmdBuffer->commandBuffers[currentFrame], 0, 1, skyboxVertexBuffers, offsets);
            vkCmdBindDescriptorSets(cmdBuffer->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    cubeMapPipeline->getPipelineLayout(), 0, 1, &cubeMapDescriptor->handle()[currentFrame], 0, nullptr);
            vkCmdPushConstants(cmdBuffer->commandBuffers[currentFrame], pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glowData), &glowData);
            vkCmdDraw(cmdBuffer->commandBuffers[currentFrame], static_cast<uint32_t>(c_model->skyBoxVertices.size()), 1, 0, 0);
        }

        ImGui_ImplVulkan_RenderDrawData(
            ImGui::GetDrawData(),
            cmdBuffer->commandBuffers[currentFrame]);

        cmdBuffer->endCmd(currentFrame);
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
    Example layer{};
    layer.runLoop();
}