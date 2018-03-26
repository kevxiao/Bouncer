#version 330

in vec3 position;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Perspective;

void main()
{
    gl_Position = Perspective * View * Model * vec4(position, 1.0);
}  