#version 460

layout(location = 0) out vec4 oColor;

void main() {
	oColor = vec4(1.0 - gl_FragDepth, 1.0 - gl_FragCoord.z, 0.0, 1.0);
}