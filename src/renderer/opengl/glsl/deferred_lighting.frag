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
out vec4 frag_color;

uniform sampler2D g_albedo;
uniform sampler2D g_normal_roughness;
uniform sampler2D g_material;
uniform sampler2D g_baked_light;
uniform sampler2D g_depth;
uniform sampler2DShadow shadow_atlas;
uniform samplerCubeShadow point_shadow_maps[4];

uniform mat4 view;
uniform mat4 projection;
uniform mat4 inverse_view;
uniform mat4 inverse_projection;
uniform vec3 cam_pos_world;
uniform vec2 texel_size;
uniform vec3 ambient_sky_color;
uniform vec3 ambient_ground_color;
uniform float ambient_intensity;
uniform bool ambient_enabled;
uniform samplerCube ibl_irradiance;
uniform samplerCube ibl_prefiltered;
uniform bool ibl_enabled;
uniform float ibl_intensity;
uniform float ibl_rotation_radians;
uniform bool fog_enabled;
uniform vec3 fog_inscattering_color;
uniform float fog_density;
uniform float fog_height_falloff;
uniform float fog_base_height;
uniform float fog_start_distance;
uniform float fog_max_opacity;
uniform float fog_cutoff_distance;

const int MAX_DIRECT_LIGHTS = 64;
const float LIGHT_TYPE_DIRECTIONAL = 0.0;
const float LIGHT_TYPE_POINT = 1.0;
const float LIGHT_TYPE_SPOT = 2.0;
const float SHADOW_TYPE_NONE = 0.0;
const float SHADOW_TYPE_SPOT = 1.0;
const float SHADOW_TYPE_DIRECTIONAL = 2.0;
const float SHADOW_TYPE_POINT = 3.0;
const float PI = 3.14159265359;

struct GpuLight {
	vec4 position_range;
	vec4 direction_spot;
	vec4 color_intensity;
	vec4 params;
};

layout(std140) uniform DirectLightBlock {
	vec4 light_info;
	GpuLight lights[MAX_DIRECT_LIGHTS];
};

layout(std140) uniform ShadowBlock {
	mat4 spot_shadow_matrices[16];
	mat4 cascade_shadow_matrices[8];
	vec4 spot_atlas_rects[16];
	vec4 spot_shadow_params[16];
	vec4 cascade_atlas_rects[8];
	vec4 cascade_shadow_params[8];
	vec4 point_shadow_params[4];
	vec4 point_filter_params[4];
	vec4 shadow_counts;
};

vec3 world_position_from_depth(float depth)
{
	vec4 clip_pos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 view_pos = inverse_projection * clip_pos;
	view_pos /= view_pos.w;
	return vec3(inverse_view * vec4(view_pos.xyz, 1.0));
}

#include "pbr_lighting.glsl"

void main()
{
	float depth = texture(g_depth, uv).r;
	if (depth >= 1.0) {
		frag_color = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	vec4 albedo_sample = texture(g_albedo, uv);
	vec4 normal_roughness = texture(g_normal_roughness, uv);
	vec4 material = texture(g_material, uv);

	vec3 albedo = albedo_sample.rgb;
	vec3 normal = normalize(normal_roughness.rgb * 2.0 - 1.0);
	float roughness = max(normal_roughness.a, 0.04);
	float metallic = material.r;
	float ao = material.g;
	bool receives_shadows = material.b > 0.5;
	bool has_lightmap = material.a > 0.5;
	vec3 world_pos = world_position_from_depth(depth);
	vec3 view_dir = normalize(cam_pos_world - world_pos);

	float sky_weight = clamp(normal.z * 0.5 + 0.5, 0.0, 1.0);
	vec3 ambient_color = mix(ambient_ground_color, ambient_sky_color, sky_weight);
	vec3 ambient = ambient_enabled && !has_lightmap ?
		ambient_color * ambient_intensity * albedo * ao : vec3(0.0);
	vec3 ibl = vec3(0.0);
	if (ibl_enabled) {
		float ndotv = max(dot(normal, view_dir), 0.0);
		vec3 f0 = mix(vec3(0.04), albedo, metallic);
		vec3 f = fresnel_schlick_roughness(ndotv, f0, roughness);
		vec3 kd = (vec3(1.0) - f) * (1.0 - metallic);
		vec3 diffuse_ibl = texture(ibl_irradiance, rotate_environment(normal)).rgb * albedo;
		vec3 reflection = reflect(-view_dir, normal);
		vec3 prefiltered = textureLod(ibl_prefiltered, rotate_environment(reflection), roughness * 4.0).rgb;
		vec2 brdf = environment_brdf(ndotv, roughness);
		vec3 specular_ibl = prefiltered * (f * brdf.x + brdf.y);
		ibl = ((has_lightmap ? vec3(0.0) : kd * diffuse_ibl * ao) + specular_ibl * ao) * ibl_intensity;
	}
	vec3 baked_indirect = albedo * texture(g_baked_light, uv).rgb;
	vec3 runtime_direct = evaluate_direct_lights(world_pos, normal, view_dir, albedo,
		metallic, roughness, receives_shadows);
	// Shadow visibility is applied inside runtime_direct only. A fully shadowed
	// mixed light must not erase the independent baked-indirect contribution.
	vec3 color = apply_height_fog(ambient + ibl + baked_indirect + runtime_direct, world_pos);

	frag_color = vec4(color, 1.0);
}
