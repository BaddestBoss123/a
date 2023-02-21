#version 460

layout(location = 0) in vec3 position;
layout(location = 4) in mat4 mvp;
layout(location = 8) in vec4 baseColorFactor;

layout(location = 0) flat out vec4 vBaseColorFactor;

void main() {
	gl_Position = mvp * vec4(position, 1.0);
	vBaseColorFactor = baseColorFactor;
}