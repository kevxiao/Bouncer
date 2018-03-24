#version 330

// Model-Space coordinates
in vec3 position;
in vec3 normal;

struct LightSource {
    vec3 position;
    vec3 rgbIntensity;
};
uniform LightSource light;
uniform LightSource light2;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Perspective;

// Remember, this is transpose(inverse(ModelView)).  Normals should be
// transformed using this matrix instead of the ModelView matrix.
uniform mat3 NormalMatrix;

out VsOutFsIn {
	vec3 position_WS; // Word-space position
	vec3 position_ES; // Eye-space position
	vec3 normal_ES;   // Eye-space normal
	LightSource light;
	vec3 light_position_WS;
	LightSource light2;
	vec3 light2_position_WS;
} vs_out;

void main() {
	vec4 pos4 = vec4(position, 1.0);
	vs_out.position_WS = (Model * pos4).xyz;

	//-- Convert position and normal to Eye-Space:
	vs_out.position_ES = (View * Model * pos4).xyz;
	vs_out.normal_ES = normalize(NormalMatrix * normal);

	vs_out.light = light;
	vs_out.light_position_WS = light.position;
	vs_out.light.position = (View * vec4(light.position, 1.0)).xyz;

	vs_out.light2 = light2;
	vs_out.light2_position_WS = light2.position;
	vs_out.light2.position = (View * vec4(light2.position, 1.0)).xyz;

	gl_Position = Perspective * View * Model * vec4(position, 1.0);
}
