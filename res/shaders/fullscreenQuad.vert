#version 450

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2(inPos.x * 0.5 + 0.5, inPos.y * 0.5 + 0.5);
    gl_Position = vec4(inPos.xyz, 1.0f);
}