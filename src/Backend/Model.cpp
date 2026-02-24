#define TINYOBJLOADER_IMPLEMENTATION
#include "Model.h"
#include <iostream>

#include <unordered_map>

Model::~Model()
{
}

void Model::loadModel(std::string MODEL_PATH)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err, warn;

    std::string basePath = "D:/Dev/Graphics Proj/Engine/res/models/ferrari/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str(), basePath.c_str()))
    {
        std::cout<<"obj load error: "<<err<<std::endl;
        throw std::runtime_error(err);
    }

    m_materials = materials;
    std::unordered_map<Model::Vertex, uint32_t> uniqueVertices{};
    
    // Track all index ranges per material to handle interleaved materials
    std::vector<std::pair<int, std::pair<uint32_t, uint32_t>>> materialRanges; // (materialId, (firstIndex, indexCount))
    
    for (auto shape : shapes)
    {
        size_t indexOffset = 0;
        int currentMaterial = -1;
        uint32_t currentRangeStart = 0;

        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); face++)
        {
            int materialId = shape.mesh.material_ids[face];
            if (materialId < 0)
                materialId = 0;

            // When material changes, save the previous range
            if (materialId != currentMaterial && currentMaterial != -1)
            {
                materialRanges.push_back({currentMaterial, {currentRangeStart, static_cast<uint32_t>(indices.size()) - currentRangeStart}});
            }

            // Start a new range if this is the first face of a material
            if (materialId != currentMaterial)
            {
                currentMaterial = materialId;
                currentRangeStart = static_cast<uint32_t>(indices.size());
            }

            size_t fv = shape.mesh.num_face_vertices[face];
            for (size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t index = shape.mesh.indices[indexOffset + v];
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

                if (index.texcoord_index >= 0)
                    vertex.texCoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]};

                if (index.normal_index >= 0)
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]};

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }

            indexOffset += fv;
        }

        // Save the final range
        if (currentMaterial != -1)
        {
            materialRanges.push_back({currentMaterial, {currentRangeStart, static_cast<uint32_t>(indices.size()) - currentRangeStart}});
        }
    }

    // Merge ranges for the same material
    std::map<int, std::vector<std::pair<uint32_t, uint32_t>>> mergedRanges;
    for (auto& [matId, range] : materialRanges)
    {
        mergedRanges[matId].push_back(range);
    }

    // Create submeshes - each range must be drawn separately since materials are interleaved
    for (auto& [matId, ranges] : mergedRanges)
    {
        // For interleaved materials, we create one submesh per contiguous range
        for (auto& [start, count] : ranges)
        {
            Submesh submesh{};
            submesh.materialIndex = matId;
            submesh.firstIndex = start;
            submesh.indexCount = count;
            
            submeshes.push_back(submesh);
        }
    }

    std::cout << "Model loaded - Total Vertices: " << vertices.size() << ", Total Indices: " << indices.size() 
              << ", Submeshes: " << submeshes.size() << std::endl;
}

    // old model loading logic
    /* for (const auto &shape : shapes)
    {
        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]};

            // std::cout << "Tex coord: " << index.texcoord_index << std::endl;
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                //1.0f -
                attrib.texcoords[2 * index.texcoord_index + 1]};

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    } */
//}
