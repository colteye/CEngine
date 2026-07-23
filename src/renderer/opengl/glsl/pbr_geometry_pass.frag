#version 330 core

layout(location = 0) out vec4 g_albedo;
layout(location = 1) out vec4 g_normal_roughness;
layout(location = 2) out vec4 g_material;
layout(location = 3) out vec3 g_baked_light;

in vec2 uv;
in vec3 normal_pos_world;
in vec3 tangent_pos_world;
in vec3 bitangent_pos_world;
in vec2 lightmap_uv;

uniform sampler2D albedo;
uniform sampler2D normal;
uniform sampler2D metallic_roughness_ao;
uniform vec4 base_color_factor;
uniform vec3 metallic_roughness_ao_factors;
uniform float alpha_cutoff;
uniform int render_mode;
uniform bool receives_shadows;
uniform sampler2D lightmap;
uniform vec4 lightmap_scale_offset;
uniform float lightmap_rgbm_range;
uniform bool has_lightmap;

const int RENDER_MODE_ALPHA_CLIP = 1;
const int RENDER_MODE_ALPHA_HASH_DITHER = 2;

vec3 calculate_normal()
{
	vec3 normal_tex = normalize(texture(normal, uv).rgb * 2.0 - 1.0);
	mat3 tbn = mat3(normalize(tangent_pos_world),
		normalize(bitangent_pos_world),
		normalize(normal_pos_world));
	return normalize(tbn * normal_tex);
}

void main()
{
	vec4 albedo_sample = texture(albedo, uv) * base_color_factor;
	if (render_mode == RENDER_MODE_ALPHA_CLIP && albedo_sample.a < alpha_cutoff) {
		discard;
	}
	if (render_mode == RENDER_MODE_ALPHA_HASH_DITHER) {
		float threshold = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
		if (albedo_sample.a < threshold) {
			discard;
		}
	}

	vec3 mra = texture(metallic_roughness_ao, uv).rgb * metallic_roughness_ao_factors;
	vec3 world_normal = calculate_normal();
	float flags = receives_shadows ? 1.0 : 0.0;

	g_albedo = albedo_sample;
	g_normal_roughness = vec4(world_normal * 0.5 + 0.5, mra.g);
	g_material = vec4(mra.r, mra.b, flags, has_lightmap ? 1.0 : 0.0);
	if (has_lightmap) {
		vec2 atlas_uv = lightmap_uv * lightmap_scale_offset.xy + lightmap_scale_offset.zw;
		g_baked_light = texture(lightmap, atlas_uv).rgb;
	} else {
		g_baked_light = vec3(0.0);
	}
}
