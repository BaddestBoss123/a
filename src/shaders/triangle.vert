#version 460

vec2 positions[] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

layout(push_constant) uniform PushConstants {
	mat4 viewProjection;
} pushConstants;

void main() {
	gl_Position = pushConstants.viewProjection * vec4(positions[gl_VertexIndex], -1.0, 1.0);
}