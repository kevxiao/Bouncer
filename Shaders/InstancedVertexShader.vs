#version 330

// Model-Space coordinates
in vec3 position;
in mat4 model;

uniform mat4 View;
uniform mat4 Perspective;

void main() {
	gl_Position = Perspective * View * model * vec4(position, 1.0);
}
