#version 460

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in mat4 mvp;

layout(location = 0) out vec2 oTexCoord;

void main() {
	gl_Position = mvp * vec4(position, 1.0);

	oTexCoord = texCoord;
}