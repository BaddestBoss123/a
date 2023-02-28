#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) flat in vec4 materialColor;
layout(location = 3) flat in uvec4 materialIndices;

layout(location = 0) out vec4 oColor;

layout(binding = 1) uniform sampler2D textures[25];

void main() {
	// oColor = vec4(texCoord, 0.0, 1.0); // materialColor;
	oColor = materialColor * texture(textures[materialIndices.x], texCoord);
}