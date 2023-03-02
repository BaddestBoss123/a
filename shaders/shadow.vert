#version 460

layout(location = 0) in vec3 position;
layout(location = 4) in mat4 mvp;

void main() {
	gl_Position = mvp * vec4(position, 1.0);
}