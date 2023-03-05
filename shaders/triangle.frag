#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) flat in vec4 materialColor;
layout(location = 3) flat in uvec4 materialIndices;

layout(location = 0) out vec4 oColor;

layout(binding = 1) uniform sampler2D textures[25];

void main() {
	vec3 N = normalize(normal);
	vec3 sun = normalize(vec3(0.36, 0.80, 0.48));

	vec3 sunColor = vec3(0.9);

	vec3 lightIntensity = vec3(0.2) + sunColor * max(dot(N, sun), 0.0);

	vec4 color = materialColor * texture(textures[materialIndices.x], texCoord);
	// oColor = vec4(texCoord, 0.0, 1.0); // materialColor;
	oColor = vec4(color.xyz * lightIntensity, color.w);
}