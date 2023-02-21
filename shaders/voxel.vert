#version 460

layout(location = 4) in mat4 mvp;
layout(location = 8) in vec4 baseColorFactor;

layout(location = 0) flat out vec4 vBaseColorFactor;

void main() {
	uint b = 1 << gl_VertexIndex;
	vec3 vertexPos = vec3((b & 0xCCF0C3) != 0, (b & 0xF6666) != 0, (b & 0x96C30F) != 0) - 0.5;

	gl_Position = mvp * vec4(vertexPos, 1.0);
	vBaseColorFactor = baseColorFactor;
}