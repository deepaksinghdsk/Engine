#version 450

layout(set=0, binding=0) uniform sampler2D imageSampler;

layout(push_constant) uniform drawData
{
    float lumThreshold;
} data;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = texture(imageSampler, inUV);
    //outColor = color;

    // Calculate luminance
    //if(glow.glowData){
    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));    //vec3(0.2126, 0.7152, 0.0722));
    vec4 result;
    if(luminance > data.lumThreshold)
        result = color;
    else
        result = vec4(0.0, 0.0, 0.0, 1.0);
    result.a = 1.0;
    outColor = result;
    //}else
}