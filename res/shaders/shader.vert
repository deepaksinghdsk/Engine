#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragVert;
layout(location = 4) out mat4 model;

void main() {
    //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    //fragColor = colors[gl_VertexIndex];
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0); //vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;

    model = ubo.model;
    fragVert = inPosition;
}
