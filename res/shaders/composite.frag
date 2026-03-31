#version 450

layout(set=0, binding=0) uniform sampler2D sceneSampler;
layout(set=0, binding=1) uniform sampler2D bloomSampler;

layout(push_constant) uniform compData
{
    bool toneMapping;
    bool gamma;
}data;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 sceneColor = texture(sceneSampler, inUV);
    vec4 bloomColor = texture(bloomSampler, inUV);

    const float gamma = 2.2;

    //Blend bloom with scene color
    float bloomIntensity = 4.0f;
    vec4 color = sceneColor + (bloomColor * bloomIntensity);

    //Tone mapping
    if(data.toneMapping)
    {
        //color.rgb = color.rgb / (color.rgb + vec3(1.0, 1.0, 1.0));    //reinhard tone mapping
        color.rgb = (vec3(1.0, 1.0, 1.0) - exp(-color.rgb * 0.58));       //Better appr to tone mapping
    }
    
    //Gamma correction
    if(data.gamma)
    {
        color.rgb = pow(color.rgb, (vec3(1.0, 1.0, 1.0)/gamma));
    }

    color.a = 1.0;
    outColor = color;

}