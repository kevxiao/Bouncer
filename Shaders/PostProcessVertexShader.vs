#version 330

// Model-Space coordinates
in vec3 position;
in vec2 texcoord_in;

out vec2 texcoord;

void main() {
	texcoord = texcoord_in;
	gl_Position = vec4(position.x, position.y, 0.0, 1.0);
}
