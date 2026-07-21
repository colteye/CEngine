#version 330 core

in vec2 uv;
layout(location = 0) out vec3 frag_color;

uniform sampler2D render_tex;
uniform sampler2D depth_tex;
uniform sampler2D ao_tex;
uniform vec2 texel_size;

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

	frag_color = color * (ao / weight);
}
