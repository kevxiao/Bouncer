#version 330

in vec2 texcoord;

out vec4 fragColor;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

uniform mat4 View;
uniform mat4 Perspective;
uniform mat4 ViewPrev;
uniform mat4 PerspectivePrev;

uniform bool MotionBlur;
uniform bool GaussianBlur;
uniform bool Darken;

const int numSamples = 10;
const float offset = 1.0 / 500.0;

// 5 x 5 kernel for blur
const vec2 offsets[25] = vec2[](
	vec2(-2 * offset, 2 * offset),
	vec2(-offset, 2 * offset),
	vec2( 0.0f, 2 * offset),
	vec2(offset, 2 * offset),
	vec2(2 * offset, 2 * offset),

	vec2(-2 * offset, offset),
	vec2(-offset, offset),
	vec2( 0.0f, offset),
	vec2(offset, offset),
	vec2(2 * offset, offset),

	vec2(-2 * offset, 0.0f),
	vec2(-offset, 0.0f),
	vec2( 0.0f, 0.0f),
	vec2(offset, 0.0f),
	vec2(2 * offset, 0.0f),

	vec2(-2 * offset, -offset),
	vec2(-offset, -offset),
	vec2( 0.0f, -offset),
	vec2(offset, -offset),
	vec2(2 * offset, -offset),

	vec2(-2 * offset, -2 * offset),
	vec2(-offset, -2 * offset),
	vec2( 0.0f, -2 * offset),
	vec2(offset, -2 * offset),
	vec2(2 * offset, -2 * offset)
);

vec3 gaussianBlur(vec2 coord) {
	float kernel[25] = float[](
		1.0 / 273, 4.0 / 273, 7.0 / 273, 4.0 / 273, 1.0 / 273,
		4.0 / 273, 16.0 / 273, 26.0 / 273, 16.0 / 273, 4.0 / 273,
		7.0 / 273, 26.0 / 273, 41.0 / 273, 26.0 / 273, 7.0 / 273,
		4.0 / 273, 16.0 / 273, 26.0 / 273, 16.0 / 273, 4.0 / 273,
		1.0 / 273, 4.0 / 273, 7.0 / 273, 4.0 / 273, 1.0 / 273
	);

	vec3 col = vec3(0.0);
	for(int i = 0; i < 25; i++)
	{
	    col += vec3(texture(screenTexture, coord.st + offsets[i])) * kernel[i];
	}

	return col;
}

vec3 motionBlur(vec3 colour, vec2 coord) {
	vec3 finalColour = colour;

	// calculate motion vector
	float depth = texture(depthTexture, coord).r;
	vec4 viewPos = vec4(coord.x * 2.0f - 1.0f, (1.0f - coord.y) * 2.0f - 1.0f, depth, 1);
	vec4 worldPos = inverse(Perspective * View) * viewPos;
	worldPos /= worldPos.w;

	vec4 viewPosPrev = PerspectivePrev * ViewPrev * worldPos;
	viewPosPrev /= viewPosPrev.w;

	vec2 velocity = -(viewPos - viewPosPrev).xy / (2.0f * numSamples);

	// do blur
	vec2 blurcoord = coord;
	for(int i = 1; i < numSamples; ++i)
	{
		blurcoord += velocity;
		finalColour += texture(screenTexture, blurcoord).rgb;
	}

	return finalColour / numSamples;
}

void main() {
	vec3 colour = texture(screenTexture, texcoord).rgb;
	if (MotionBlur) {
		colour = motionBlur(colour, texcoord);
	}
	if (GaussianBlur) {
		colour = gaussianBlur(texcoord);
	}
	if (Darken) {
		colour = colour * 0.5;
	}
	fragColor = vec4( colour, 1 );
}
