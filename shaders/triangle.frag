#version 460

layout(location = 0) in vec3 vNormal;
layout(location = 1) flat in vec4 vBaseColorFactor;

layout(location = 0) out vec4 oColor;

void main() {
	oColor = vec4(vNormal * 0.5 + 0.5, 1.0); // vBaseColorFactor;
}