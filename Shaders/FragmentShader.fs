#version 330

struct LightSource {
    vec3 position;
    vec3 rgbIntensity;
};

in VsOutFsIn {
    vec3 position_WS; // Word-space position
	vec3 position_ES; // Eye-space position
	vec3 normal_ES;   // Eye-space normal
	LightSource light;
    vec3 light_position_WS;
    LightSource light2;
    vec3 light2_position_WS;
} fs_in;

out vec4 fragColour;

struct Material {
    vec3 kd;
    vec3 ks;
    float shininess;
};
uniform Material material;

// Ambient light intensity for each RGB component.
uniform vec3 ambientIntensity;

uniform float farPlane;

uniform samplerCube shadowMap;
uniform samplerCube shadowMap2;

// array of offset direction for sampling
vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float calcShadow(samplerCube sMap, vec3 fragPosition, vec3 lightPos)
{
    vec3 fragToLight = fragPosition - lightPos;
    float currentDepth = length(fragToLight);
    float shadow = 0.0;
    float bias = 0.05;
    int samples = 20;
    float viewDistance = length(-fragPosition);
    float diskRadius = (1.0 + (viewDistance / farPlane)) / farPlane;
    for(int i = 0; i < samples; ++i) {
        float closestDepth = texture(sMap, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= farPlane;
        if(currentDepth - bias > closestDepth) {
            shadow += 1.0;
        }
    }
    shadow /= float(samples);
    shadow = 1.0 - shadow;
        
    return shadow;
}

vec3 pointLighting(LightSource light, vec3 lightPos, samplerCube sMap, vec3 fragPosition, vec3 fragNormal) {
    // calculate light attenuation over distance
    float distance = length(light.position - fragPosition);
    float att = 1.0 / (1.0 + 0.007 * distance + 0.0002 * (distance * distance));

    // Direction from fragment to light source.
    vec3 l = normalize(light.position - fragPosition);

    // Direction from fragment to viewer (origin - fragPosition).
    vec3 v = normalize(-fragPosition.xyz);

    float n_dot_l = max(dot(fragNormal, l), 0.0);

    vec3 diffuse;
    diffuse = material.kd * n_dot_l;

    vec3 specular = vec3(0.0);

    if (n_dot_l > 0.0) {
        // Halfway vector.
        vec3 h = normalize(v + l);
        float n_dot_h = max(dot(fragNormal, h), 0.0);

        specular = material.ks * pow(n_dot_h, material.shininess);
    }

    specular *= att;
    diffuse *= att;

    float shadow = calcShadow(sMap, fs_in.position_WS, lightPos);

    return light.rgbIntensity * (diffuse + specular) * shadow;
}

vec3 phongModel(vec3 fragPosition, vec3 fragNormal) {
    LightSource light = fs_in.light;

    vec3 lighting1 = pointLighting(fs_in.light, fs_in.light_position_WS, shadowMap, fragPosition, fragNormal);
    vec3 lighting2 = pointLighting(fs_in.light2, fs_in.light2_position_WS, shadowMap2, fragPosition, fragNormal);

    vec3 ambient = material.kd;

    return ambientIntensity * ambient + lighting1 + lighting2;
}

void main() {
    fragColour = vec4(phongModel(fs_in.position_ES, fs_in.normal_ES), 1.0);
}
