#version 450
#define MAX_TEXTURES 53

layout(binding = 1) uniform sampler2D texSamplers[MAX_TEXTURES];
layout(binding = 2) uniform Light{
	vec3 position;
	vec3 intensities; // the color of the light
	float attenuation;
	float ambientCoefficient;
} light;

layout(push_constant) uniform DrawData
{
	uint texIndex;
} draw;

vec3 cameraPosition = {0.0f, 0.0f, 7.0f};
float materialShininess = 0.01f;
vec3 materialSpecularColor= {1.0f, 1.0f, 1.0f};

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragVert;
layout(location = 4) in mat4 model;

layout(location = 0) out vec4 outColor;

void main() {
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = vec4(0.246, 0.246, 0.246, 1.0);
    //outColor = texture(texSampler, fragTexCoord);

    //Phong shading
    vec3 surfacePosition = vec3(model * vec4(fragVert, 1));

	//calculate normal in world coordinates
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 normal = normalize(normalMatrix * fragNormal);

	//calculate the location of this fragment (pixel) in world coordinates
	vec3 fragPosition = vec3(model * vec4(fragVert, 1));

	//calculate the vector from this pixel to the light source
	vec3 surfaceToLight = light.position - fragPosition;

	//brightness/cos(angle)
	float brightness = dot(normal, surfaceToLight) / (length(surfaceToLight) * length(normal));
	brightness = clamp(brightness, 0, 100);

	//specular
	vec3 incidenceVector = -surfaceToLight; //a unit vector
	vec3 reflectionVector = reflect(incidenceVector, normal); //also a unit vector
	vec3 surfaceToCamera = normalize(cameraPosition - surfacePosition); //also a unit vector
	float cosAngle = max(0.0, dot(surfaceToCamera, reflectionVector));
	float specularCoefficient = pow(cosAngle, materialShininess);

	//calculate final color of the pixel, based on:
	// 1. The angle of incidence: brightness
	// 2. The color/intensities of the light: light.intensities
	// 3. The texture and texture coord: texture(tex, fragTexCoord)
	
	vec4 surfaceColor = texture(texSamplers[draw.texIndex], fragTexCoord); //vec4(0.246, 0.246, 0.246, 1.0);

	vec3 diffuse = 20 * brightness * light.intensities * surfaceColor.rgb;
	
	vec3 ambient = light.ambientCoefficient * surfaceColor.rgb * light.intensities;

	vec3 specular = specularCoefficient * materialSpecularColor * light.intensities;

	float distanceToLight = length(light.position - surfacePosition);
	float attenuation1 = 1.0 / (1.0 + light.attenuation * pow(distanceToLight, 2));

	//linear color(color before gamma correction)
	vec3 linearColor = ambient + attenuation1 * (diffuse + specular);
	
	vec3 gamma = vec3(1.0/2.2);
	linearColor = pow(linearColor, gamma);
	outColor = vec4(linearColor, surfaceColor.a);
}
