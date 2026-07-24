//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core

in vec2 uv;
layout(location = 0) out vec3 frag_color;

uniform sampler2D render_tex;
uniform sampler2D depth_tex;
uniform sampler2D ao_tex;
uniform vec2 texel_size;
uniform mat4 inverse_projection;
uniform bool fog_enabled;
uniform float fog_density;
uniform float fog_start_distance;

float view_distance(vec2 sample_uv, float depth)
{
	vec4 clip_pos = vec4(sample_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 view_pos = inverse_projection * clip_pos;
	return length(view_pos.xyz / view_pos.w);
}

void main()
{
	vec3 color = texture(render_tex, uv).rgb;
	float center_depth = texture(depth_tex, uv).r;
	if (center_depth >= 1.0)
	{
		frag_color = color;
		return;
	}

	float ao = texture(ao_tex, uv).r * 4.0;
	float weight = 4.0;
	const vec2 offsets[4] = vec2[4](
		vec2(1.0, 0.0), vec2(-1.0, 0.0),
		vec2(0.0, 1.0), vec2(0.0, -1.0));
	for (int i = 0; i < 4; ++i)
	{
		vec2 sample_uv = clamp(uv + offsets[i] * texel_size, vec2(0.0), vec2(1.0));
		float sample_depth = texture(depth_tex, sample_uv).r;
		float edge_weight = exp(-abs(center_depth - sample_depth) * 800.0);
		ao += texture(ao_tex, sample_uv).r * edge_weight;
		weight += edge_weight;
	}

	float filtered_ao = ao / weight;
	// AO is surface-local. As in-scattering replaces the surface color, fade AO
	// out as the fog becomes optically dense so it cannot darken the fog layer.
	if (fog_enabled) {
		float fog_distance = max(0.0, view_distance(uv, center_depth) - fog_start_distance);
		float fog_amount = 1.0 - exp(-max(fog_density, 0.0) * fog_distance);
		filtered_ao = mix(filtered_ao, 1.0, fog_amount);
	}
	frag_color = color * filtered_ao;
}
