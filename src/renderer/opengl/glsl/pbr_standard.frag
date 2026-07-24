//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core
out vec4 frag_color;

// Data from vertex shader.
in vec2 uv;
in vec2 lightmap_uv;
in vec3 vertex_pos_world;
in vec3 normal_pos_world;
in vec3 tangent_pos_world;
in vec3 bitangent_pos_world;

// Material parameters.
uniform sampler2D albedo;
uniform sampler2D normal;
uniform sampler2D metallic_roughness_ao;
uniform sampler2DShadow shadow_atlas;
uniform samplerCubeShadow point_shadow_maps[4];
uniform sampler2D lightmap;

uniform vec3 cam_pos_world;
uniform vec4 base_color_factor;
uniform vec3 metallic_roughness_ao_factors;
uniform float alpha_cutoff;
uniform int render_mode;
uniform bool receives_shadows;
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
uniform mat4 view;
uniform vec4 lightmap_scale_offset;
uniform float lightmap_rgbm_range;
uniform bool has_lightmap;

const int MAX_DIRECT_LIGHTS = 64;
const float LIGHT_TYPE_DIRECTIONAL = 0.0;
const float LIGHT_TYPE_POINT = 1.0;
const float LIGHT_TYPE_SPOT = 2.0;
const float SHADOW_TYPE_NONE = 0.0;
const float SHADOW_TYPE_SPOT = 1.0;
const float SHADOW_TYPE_DIRECTIONAL = 2.0;
const float SHADOW_TYPE_POINT = 3.0;
const int RENDER_MODE_ALPHA_CLIP = 1;
const int RENDER_MODE_ALPHA_HASH_DITHER = 2;
const int RENDER_MODE_UNLIT = 4;

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

const float PI = 3.14159265359;

vec3 CalculateNormals()
{
	// Preprocess tangent space normal map.
    vec3 normal_tex = normalize(texture(normal, uv).rgb *2.0 - 1.0);
	
    mat3 TBN = mat3(normalize(tangent_pos_world), 
	normalize(bitangent_pos_world), 
	normalize(normal_pos_world));
	
	// Convert normal from tangent space into world space.
    return normalize(TBN * normal_tex);
}

#include "pbr_lighting.glsl"
#include "environment_probes.glsl"

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

    vec3 albedo = albedo_sample.rgb;
	
    vec3 mra = texture(metallic_roughness_ao, uv).rgb * metallic_roughness_ao_factors;
    float metallic = clamp(mra.r, 0.0, 1.0);
    float roughness = clamp(mra.g, 0.04, 1.0);
    float ao = clamp(mra.b, 0.0, 1.0);

    if (render_mode == RENDER_MODE_UNLIT) {
        frag_color = vec4(albedo, albedo_sample.a);
        return;
    }

    vec3 N = CalculateNormals();

    vec3 V = normalize(cam_pos_world - vertex_pos_world);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = evaluate_direct_lights(
        vertex_pos_world, N, V, albedo, metallic, roughness,
        receives_shadows);
  
    float sky_weight = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient_color = mix(ambient_ground_color, ambient_sky_color, sky_weight);
	vec3 ambient = ambient_enabled && !has_lightmap ?
		ambient_color * ambient_intensity * albedo * ao : vec3(0.0);
	vec3 global_ibl = vec3(0.0);
	if (ibl_enabled) {
		float ndotv = max(dot(N, V), 0.0);
		vec3 f = fresnel_schlick_roughness(ndotv, F0, roughness);
		vec3 kd = (vec3(1.0) - f) * (1.0 - metallic);
		vec3 diffuse_ibl = texture(
            ibl_irradiance, rotate_environment(N)).rgb * albedo;
		vec3 reflection = reflect(-V, N);
		vec3 prefiltered = textureLod(
            ibl_prefiltered, rotate_environment(reflection),
            roughness * 4.0).rgb;
		vec2 brdf = environment_brdf(ndotv, roughness);
		vec3 specular_ibl = prefiltered * (f * brdf.x + brdf.y);
		// The global environment has no local visibility. Lightmapped surfaces
		// contain their World diffuse transport already and receive no global
		// runtime IBL; spatial reflection probes can add specular separately.
		global_ibl = has_lightmap
			? vec3(0.0)
			: (kd * diffuse_ibl * ao + specular_ibl * ao) * ibl_intensity;
	}
	float probe_coverage = 0.0;
	vec3 probe_ibl = has_lightmap ? vec3(0.0) :
		evaluate_environment_probes(vertex_pos_world, N, V, albedo,
			metallic, roughness, ao, probe_coverage);
	vec3 ibl = probe_ibl + global_ibl * (1.0 - probe_coverage);
	ambient *= 1.0 - probe_coverage;
	vec3 baked_irradiance = vec3(0.0);
	if (has_lightmap) {
		vec2 atlas_uv = lightmap_uv * lightmap_scale_offset.xy + lightmap_scale_offset.zw;
		baked_irradiance = texture(lightmap, atlas_uv).rgb;
	}
	vec3 baked_indirect = albedo * baked_irradiance;
	// Per-light shadow visibility is applied to Lo only; baked GI remains
	// visible when a mixed light's realtime direct term is fully shadowed.
    vec3 color = apply_height_fog(
        ambient + ibl + baked_indirect + Lo, vertex_pos_world);

    frag_color = vec4(color, albedo_sample.a);
}  
