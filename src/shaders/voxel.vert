#version 460

layout(location = 4) in mat4 mvp;

void main() {
	// uint face = voxelFaces.f[gl_VertexIndex / 4];

	// xyz position in integers from 0 to 15
	// vec3 blockPos = vec3(face & 0xF, (face >> 4) & 0xF, (face >> 8) & 0xF);

	// uint faceOrientation = face >> 12;
	// uint vertexIndex = (faceOrientation * 4) + (gl_VertexIndex % 4);

	// dark magic
	uint b = 1 << gl_VertexIndex;
	vec3 vertexPos = vec3((b & 0xCCF0C3) != 0, (b & 0xF6666) != 0, (b & 0x96C30F) != 0) - 0.5;

	gl_Position = mvp * vec4(vertexPos, 1.0);
}