#version 460

layout(location = 0) in vec3 position;

layout(push_constant) uniform PushConstants {
	mat4 viewProjection;
} pushConstants;

void main() {
	gl_Position = pushConstants.viewProjection * vec4(position, 1.0);
	gl_PointSize = 400.0 / gl_Position.w;
}