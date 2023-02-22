#version 460

layout(location = 4) in mat4 world;
layout(location = 8) in vec4 baseColorFactor;

layout(location = 0) flat out vec4 vBaseColorFactor;

layout(push_constant) uniform PushConstants {
	mat4 viewProjection;
	vec4 clippingPlane;
} pushConstants;

void main() {
	uint b = 1 << gl_VertexIndex;
	vec3 vertexPos = vec3((b & 0xCCF0C3) != 0, (b & 0xF6666) != 0, (b & 0x96C30F) != 0) - 0.5;

	vec4 worldPos = world * vec4(vertexPos, 1.0);
	gl_ClipDistance[0] = dot(pushConstants.clippingPlane, worldPos);
	gl_Position = pushConstants.viewProjection * worldPos;

	vBaseColorFactor = baseColorFactor;
}