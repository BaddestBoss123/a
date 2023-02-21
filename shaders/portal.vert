#version 460

layout(push_constant) uniform PushConstants {
	mat4 mvp;
} pushConstants;

vec2 positions[] = vec2[](
	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),
	vec2(-1.0,  1.0),
	vec2(-1.0,  1.0),
	vec2( 1.0, -1.0),
	vec2( 1.0,  1.0)
);

void main() {
	gl_Position = pushConstants.mvp * vec4(positions[gl_VertexIndex], 0.0, 1.0);
}