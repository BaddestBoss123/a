#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 4) in mat4 mvp;
layout(location = 8) in vec4 baseColorFactor;

layout(location = 0) flat out vec3 vNormal;
layout(location = 1) flat out vec4 vBaseColorFactor;

layout(push_constant) uniform PushConstants {
	vec4 clippingPlane;
} pushConstants;

void main() {
	gl_Position = mvp * vec4(position, 1.0);
	gl_ClipDistance[0] = dot(pushConstants.clippingPlane, gl_Position);

	vNormal = normal;
	vBaseColorFactor = baseColorFactor;
}