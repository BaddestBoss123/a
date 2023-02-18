#version 460

layout(location = 0) out vec2 vTexCoord;

void main() {
	vTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vTexCoord.x * 2.0 - 1.0, vTexCoord.y * 2.0 - 1.0, 0.0, 1.0);
}