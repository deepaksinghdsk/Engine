#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outUVW;

void main()
{
    outUVW = inPos;
    // Convert cubemap coordinates into Vulkan coordinate space
    //outUVW *= -1.0;
    // Remove translation from view matrix
	mat4 viewMat = mat4(mat3(ubo.view));
    vec4 pos = ubo.proj * viewMat * vec4(inPos, 1.0);
    gl_Position = pos.xyww;
}