#version 450

layout(set=0, binding=0) uniform sampler2D imageSampler;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    //vec4 color = texture(imageSampler, inUV);

    vec2 texelSize = vec2(1.0 / 1280.0, 1.0 / 720.0);
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    
    // Gaussian blur weights
    float weights[4] = { 0.4026, 0.2441, 0.0554, 0.0038 };
    
    color = texture(imageSampler, inUV) * weights[0];
    color += texture(imageSampler, inUV + vec2(texelSize.y, 0.0)) * weights[1];
    color += texture(imageSampler, inUV - vec2(texelSize.y, 0.0)) * weights[1];
    color += texture(imageSampler, inUV + vec2(texelSize.y * 2.0, 0.0)) * weights[2];
    color += texture(imageSampler, inUV - vec2(texelSize.y * 2.0, 0.0)) * weights[2];
    color += texture(imageSampler, inUV + vec2(texelSize.y * 3.0, 0.0)) * weights[3];
    color += texture(imageSampler, inUV - vec2(texelSize.y * 3.0, 0.0)) * weights[3];
    
    color.a = 1.0;
    outColor = color;
}