#version 460

layout(location = 0) flat in vec4 vBaseColorFactor;

layout(location = 0) out vec4 oColor;

void main() {
	oColor = vBaseColorFactor;
}