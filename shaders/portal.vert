#version 460

layout(push_constant) uniform PushConstants {
	mat4 mvp;
} pushConstants;

void main() {
	float x = -1.0 + 2.0 * float(gl_VertexIndex % 2);
	float y = -1.0 + 2.0 * float(gl_VertexIndex / 2);

	gl_Position = pushConstants.mvp * vec4(x, y, 0.0, 1.0);
}