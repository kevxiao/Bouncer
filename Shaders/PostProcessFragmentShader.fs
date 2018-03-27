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

// samples for motion blur
const int numSamples = 20;

// 5 x 5 kernel for gaussian blur
const float offset = 1.0 / 250.0;
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
const float kernel[25] = float[](
	2.0 / 115, 4.0 / 115, 5.0 / 115, 4.0 / 115, 2.0 / 115,
	4.0 / 115, 9.0 / 115, 12.0 / 115, 9.0 / 115, 4.0 / 115,
	5.0 / 115, 12.0 / 115, 15.0 / 115, 12.0 / 115, 5.0 / 115,
	4.0 / 115, 9.0 / 115, 12.0 / 115, 9.0 / 115, 4.0 / 115,
	2.0 / 115, 4.0 / 115, 5.0 / 115, 4.0 / 115, 2.0 / 115
);

vec3 gaussianBlur(vec2 coord) {
	vec3 col = vec3(0.0);
	for(int i = 0; i < 25; i++)
	{
	    col += vec3(texture(screenTexture, coord.st + offsets[i])) * kernel[i];
	}

	return col;
}

vec3 motionBlur(vec2 coord) {
	vec3 colour = vec3(0.0f);

	// calculate motion vector
	float depth = texture(depthTexture, coord).r;
	vec4 viewPos = vec4(coord.x * 2.0f - 1.0f, -((1.0f - coord.y) * 2.0f - 1.0f), depth, 1);
	vec4 worldPos = inverse(Perspective * View) * viewPos;
	worldPos /= worldPos.w;

	vec4 viewPosPrev = PerspectivePrev * ViewPrev * worldPos;
	viewPosPrev /= viewPosPrev.w;

	vec2 velocity = (viewPos - viewPosPrev).xy / (2.0f * numSamples);

	// do blur
	vec2 blurcoord = coord;
	for(int i = 0; i < numSamples; ++i)
	{
		colour += texture(screenTexture, blurcoord).rgb;
		blurcoord += velocity;
	}

	return colour / numSamples;
}

void main() {
	vec3 colour = texture(screenTexture, texcoord).rgb;
	if (MotionBlur) {
		colour = motionBlur(texcoord);
	}
	if (GaussianBlur) {
		colour = gaussianBlur(texcoord);
	}
	if (Darken) {
		colour = colour * 0.5;
	}
	fragColor = vec4( colour, 1 );
}
