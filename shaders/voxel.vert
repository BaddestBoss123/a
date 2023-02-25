#version 460

layout(location = 4) in mat4 mvp;
layout(location = 8) in vec4 baseColorFactor;

layout(location = 0) out vec3 vNormal;
layout(location = 1) flat out vec4 vBaseColorFactor;

layout(push_constant) uniform PushConstants {
	vec4 clippingPlane;
} pushConstants;

vec3 normals[] = vec3[](
	vec3(0.0, 0.0, -1.0),
	vec3(0.0, 0.0, 1.0),
	vec3(0.0, -1.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(-1.0, 0.0, 0.0),
	vec3(1.0, 0.0, 0.0)
);

void main() {
	uint b = 1 << gl_VertexIndex;
	vec3 vertexPos = vec3((b & 0xCCF0C3) != 0, (b & 0xF6666) != 0, (b & 0x96C30F) != 0) - 0.5;

	gl_Position = mvp * vec4(vertexPos, 1.0);
	gl_ClipDistance[0] = dot(pushConstants.clippingPlane, gl_Position);

	vNormal = normals[(gl_VertexIndex / 4) % 6];
	vBaseColorFactor = baseColorFactor;
}