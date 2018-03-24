#version 330

in vec3 position;

uniform mat4 Model;

void main()
{
    gl_Position = Model * vec4(position, 1.0);
}  