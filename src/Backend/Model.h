#pragma once
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <tiny_obj_loader.h>

class Model
{
public:
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
        glm::vec3 normal;

        bool operator==(const Vertex &other) const
        {
            return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
        }

        static VkVertexInputBindingDescription getBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions{4};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(Vertex, normal);

            return attributeDescriptions;
        }
    };

    struct skyBoxVertex
    {
        glm::vec3 position;

        static VkVertexInputBindingDescription getBindingDesc()
        {
            VkVertexInputBindingDescription bindingDesc{};
            bindingDesc.binding = 0;
            bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDesc.stride = sizeof(skyBoxVertex);
            return bindingDesc;
        }

        static std::vector<VkVertexInputAttributeDescription> getAttributeDesc()
        {
            std::vector<VkVertexInputAttributeDescription> attribDesc{1};
            attribDesc[0].binding = 0;
            attribDesc[0].location = 0;
            attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribDesc[0].offset = offsetof(skyBoxVertex, position);

            return attribDesc;
        }
    };

    const std::vector<skyBoxVertex> skyBoxVertices = {
        // positions
        {{-1.0f, 1.0f, -1.0f}},
        {{-1.0f, -1.0f, -1.0f}},
        {{1.0f, -1.0f, -1.0f}},
        {{1.0f, -1.0f, -1.0f}},
        {{1.0f, 1.0f, -1.0f}},
        {{-1.0f, 1.0f, -1.0f}},

        {{-1.0f, -1.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}},
        {{-1.0f, 1.0f, -1.0f}},
        {{-1.0f, 1.0f, -1.0f}},
        {{-1.0f, 1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}},

        {{1.0f, -1.0f, -1.0f}},
        {{1.0f, -1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}},
        {{1.0f, -1.0f, -1.0f}},

        {{-1.0f, -1.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}},

        {{-1.0f, 1.0f, -1.0f}},
        {{1.0f, 1.0f, -1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}},
        {{-1.0f, 1.0f, -1.0f}},

        {{-1.0f, -1.0f, -1.0f}},
        {{-1.0f, -1.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}},
        {{1.0f, -1.0f, -1.0f}},
        {{-1.0f, -1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}}
    };

    struct Submesh
    {
        uint32_t firstIndex; // offset
        uint32_t indexCount;
        uint32_t materialIndex;
    };

public:
    Model() = default;
    ~Model();

    Model(Model &) = delete;
    Model &operator=(Model &) = delete;

    void loadModel(std::string MODEL_PATH);

    const std::vector<Vertex> &getVertices() { return vertices; }
    const std::vector<uint32_t> &getIndices() { return indices; }
    const std::vector<Submesh> &getSubmeshes() { return submeshes; }
    const tinyobj::material_t &getMaterial(int matInd) { return m_materials[matInd]; }

private:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
    std::vector<tinyobj::material_t> m_materials;
};

namespace std
{
    template <>
    struct hash<Model::Vertex>
    {
        size_t operator()(Model::Vertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos) ^
                     (hash<glm::vec3>()(vertex.color) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}