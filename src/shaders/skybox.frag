#version 460

layout(location = 0) in vec3 vDir;

layout(location = 0) out vec4 oColor;

void main() {
	oColor = vec4(vDir * 0.5 + 0.5, 1.0);
}