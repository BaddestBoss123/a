#version 460

layout(location = 0) out vec3 vDir;

layout(push_constant) uniform PushConstants {
	mat4 viewProjection;
} pushConstants;

void main() {
	gl_Position = vec4(((gl_VertexIndex << 1) & 2) * 2.0 - 1.0, (gl_VertexIndex & 2) * 2.0 - 1.0, 0.0, 1.0);
	vDir = (inverse(pushConstants.viewProjection) * gl_Position).xyz;
}