#version 460
#extension GL_EXT_samplerless_texture_functions : require

layout(binding = 0) uniform texture2D image;

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 oColor;

void main() {
	// todo: tone mapping
	oColor = texelFetch(image, ivec2(vTexCoord * textureSize(image, 0)), 0);
}