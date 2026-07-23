//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core

in vec2 uv;

uniform sampler2D albedo;
uniform vec4 base_color_factor;
uniform float alpha_cutoff;
uniform bool alpha_test;

void main()
{
	if (!alpha_test) {
		return;
	}

	float alpha = texture(albedo, uv).a * base_color_factor.a;
	if (alpha < alpha_cutoff) {
		discard;
	}
}
