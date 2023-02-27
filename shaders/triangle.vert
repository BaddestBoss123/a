#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in mat4 mvp;
layout(location = 8) in vec4 baseColorFactor;
layout(location = 9) in uint materialIndices;

layout(location = 0) flat out vec3 oNormal;
layout(location = 1) flat out vec2 oTexCoord;
layout(location = 2) flat out vec4 oBaseColorFactor;
layout(location = 3) flat out uvec4 oMaterialIndices;

layout(push_constant) uniform PushConstants {
	vec4 clippingPlane;
} pushConstants;

void main() {
	gl_Position = mvp * vec4(position, 1.0);
	gl_ClipDistance[0] = dot(pushConstants.clippingPlane, gl_Position);

	oNormal = normal;
	oTexCoord = texCoord;
	oBaseColorFactor = baseColorFactor;
	oMaterialIndices = uvec4(materialIndices, 0, 0, 0);
}