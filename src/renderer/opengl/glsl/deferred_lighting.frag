#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D g_albedo;
uniform sampler2D g_normal_roughness;
uniform sampler2D g_material;
uniform sampler2D g_baked_light;
uniform sampler2D g_depth;
uniform sampler2DShadow shadow_atlas;
uniform samplerCubeShadow point_shadow_maps[8];

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
	vec4 point_shadow_params[8];
	vec4 point_filter_params[8];
	vec4 shadow_counts;
};

float distribution_ggx(vec3 normal, vec3 half_vector, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float ndoth = max(dot(normal, half_vector), 0.0);
	float ndoth2 = ndoth * ndoth;
	float denominator = ndoth2 * (a2 - 1.0) + 1.0;
	return a2 / max(PI * denominator * denominator, 0.0001);
}

float geometry_schlick_ggx(float ndotv, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	return ndotv / max(ndotv * (1.0 - k) + k, 0.0001);
}

float geometry_smith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness)
{
	return geometry_schlick_ggx(max(dot(normal, view_dir), 0.0), roughness) *
		geometry_schlick_ggx(max(dot(normal, light_dir), 0.0), roughness);
}

vec3 fresnel_schlick(float hdotv, vec3 f0)
{
	return f0 + (1.0 - f0) * pow(1.0 - hdotv, 5.0);
}

vec3 world_position_from_depth(float depth)
{
	vec4 clip_pos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 view_pos = inverse_projection * clip_pos;
	view_pos /= view_pos.w;
	return vec3(inverse_view * vec4(view_pos.xyz, 1.0));
}

float atlas_shadow(mat4 light_matrix, vec4 rect, vec4 params, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	// Move the receiver toward the light in world units. The previous normal
	// bias was multiplied by a fixed depth-space constant, so it was effectively
	// inert and left self-shadowing quantization visible on lit surfaces.
	vec3 receiver_pos = world_pos + light_dir * params.y;
	vec4 clip_pos = light_matrix * vec4(receiver_pos, 1.0);
	vec3 projected = clip_pos.xyz / clip_pos.w;
	vec2 shadow_uv = projected.xy * 0.5 + 0.5;
	float current_depth = projected.z * 0.5 + 0.5;
	if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
		current_depth < 0.0 || current_depth > 1.0) {
		return 1.0;
	}

	vec2 atlas_uv = rect.xy + shadow_uv * rect.zw;
	float bias = max(params.x * (1.0 - clamp(dot(normal, light_dir), 0.0, 1.0)), params.x * 0.25);
	float atlas_texel = 1.0 / shadow_counts.w;
	vec2 atlas_min = rect.xy + vec2(atlas_texel);
	vec2 atlas_max = rect.xy + rect.zw - vec2(atlas_texel);
	vec2 offsets[4] = vec2[](
		vec2(-0.75, -0.75), vec2(0.75, -0.75),
		vec2(-0.75, 0.75), vec2(0.75, 0.75)
	);
	float lit = 0.0;
	for (int sample_index = 0; sample_index < 4; ++sample_index) {
		vec2 sample_uv = clamp(atlas_uv + offsets[sample_index] * atlas_texel, atlas_min, atlas_max);
		lit += texture(shadow_atlas, vec3(sample_uv, current_depth - bias));
	}
	return lit * 0.25;
}

float sample_point_map(int index, vec3 direction, float reference_depth)
{
	vec4 sample_coord = vec4(direction, reference_depth);
	if (index == 0) return texture(point_shadow_maps[0], sample_coord);
	if (index == 1) return texture(point_shadow_maps[1], sample_coord);
	if (index == 2) return texture(point_shadow_maps[2], sample_coord);
	if (index == 3) return texture(point_shadow_maps[3], sample_coord);
	if (index == 4) return texture(point_shadow_maps[4], sample_coord);
	if (index == 5) return texture(point_shadow_maps[5], sample_coord);
	if (index == 6) return texture(point_shadow_maps[6], sample_coord);
	return texture(point_shadow_maps[7], sample_coord);
}

float point_shadow(int index, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	vec4 point_data = point_shadow_params[index];
	vec4 filter_data = point_filter_params[index];
	vec3 receiver_pos = world_pos + light_dir * filter_data.y;
	vec3 from_light = receiver_pos - point_data.xyz;
	float current_distance = length(from_light);
	float far_plane = point_data.w;
	if (far_plane <= 0.0 || current_distance >= far_plane) {
		return 1.0;
	}

	float bias = max(filter_data.x * (1.0 - clamp(dot(normal, light_dir), 0.0, 1.0)), filter_data.x * 0.25);
	// Shadow bias is a normalized depth offset for every shadow-map type. Keep
	// the point-light path consistent with the atlas path instead of treating it
	// as world units and dividing it by the light range a second time.
	float reference_depth = clamp(current_distance / far_plane - bias, 0.0, 1.0);
	float disk_radius = 3.0 / max(filter_data.z, 64.0);
	vec3 offsets[4] = vec3[](
		vec3(1.0, 1.0, 1.0),
		vec3(1.0, -1.0, -1.0),
		vec3(-1.0, 1.0, -1.0),
		vec3(-1.0, -1.0, 1.0)
	);

	float lit = 0.0;
	vec3 direction = from_light / current_distance;
	for (int sample_index = 0; sample_index < 4; ++sample_index) {
		lit += sample_point_map(index,
			normalize(direction + offsets[sample_index] * disk_radius), reference_depth);
	}
	return lit * 0.25;
}

float directional_shadow(int cascade_start, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	float view_depth = -(view * vec4(world_pos, 1.0)).z;
	int selected = -1;
	for (int cascade = 0; cascade < 4; ++cascade) {
		int index = cascade_start + cascade;
		if (view_depth <= cascade_shadow_params[index].x) {
			selected = index;
			break;
		}
	}
	if (selected < 0) {
		return 1.0;
	}

	float shadow = atlas_shadow(cascade_shadow_matrices[selected], cascade_atlas_rects[selected],
		vec4(cascade_shadow_params[selected].y, cascade_shadow_params[selected].z, 0.0, 0.0),
		world_pos, normal, light_dir);
	float blend_range = cascade_shadow_params[selected].w;
	if (blend_range > 0.0 && selected + 1 < cascade_start + 4) {
		float split = cascade_shadow_params[selected].x;
		float blend = smoothstep(0.0, 1.0, clamp((view_depth - (split - blend_range)) / blend_range, 0.0, 1.0));
		float next_shadow = atlas_shadow(cascade_shadow_matrices[selected + 1], cascade_atlas_rects[selected + 1],
			vec4(cascade_shadow_params[selected + 1].y, cascade_shadow_params[selected + 1].z, 0.0, 0.0),
			world_pos, normal, light_dir);
		shadow = mix(shadow, next_shadow, blend);
	}
	return shadow;
}

float light_shadow(GpuLight light, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	int index = int(light.params.z + 0.5);
	float shadow_type = light.params.w;
	if (index < 0 || abs(shadow_type - SHADOW_TYPE_NONE) < 0.5) {
		return 1.0;
	}
	if (abs(shadow_type - SHADOW_TYPE_SPOT) < 0.5) {
		return atlas_shadow(spot_shadow_matrices[index], spot_atlas_rects[index], spot_shadow_params[index],
			world_pos, normal, light_dir);
	}
	if (abs(shadow_type - SHADOW_TYPE_DIRECTIONAL) < 0.5) {
		return directional_shadow(index, world_pos, normal, light_dir);
	}
	if (abs(shadow_type - SHADOW_TYPE_POINT) < 0.5) {
		return point_shadow(index, world_pos, normal, light_dir);
	}
	return 1.0;
}

vec3 evaluate_direct_lights(vec3 world_pos, vec3 normal, vec3 view_dir, vec3 albedo, float metallic,
	float roughness, bool receives_shadows)
{
	vec3 f0 = mix(vec3(0.04), albedo, metallic);
	vec3 outgoing = vec3(0.0);
	int light_count = min(int(light_info.x), MAX_DIRECT_LIGHTS);

	for (int i = 0; i < light_count; ++i) {
		GpuLight light = lights[i];
		float light_type = light.params.x;
		float attenuation = 1.0;
		float spot_factor = 1.0;
		vec3 light_dir = vec3(0.0);

		if (abs(light_type - LIGHT_TYPE_DIRECTIONAL) < 0.5) {
			light_dir = normalize(-light.direction_spot.xyz);
		} else {
			vec3 light_delta = light.position_range.xyz - world_pos;
			float distance = length(light_delta);
			if (distance <= 0.0001) {
				continue;
			}

			light_dir = light_delta / distance;
			attenuation = 1.0 / (distance * distance);

			float range = light.position_range.w;
			if (range > 0.0) {
				float range_factor = clamp(1.0 - distance / range, 0.0, 1.0);
				attenuation *= range_factor * range_factor;
			}

			if (abs(light_type - LIGHT_TYPE_SPOT) < 0.5) {
				float theta = dot(normalize(-light_dir), normalize(light.direction_spot.xyz));
				float inner_cos = light.params.y;
				float outer_cos = light.direction_spot.w;
				float epsilon = max(inner_cos - outer_cos, 0.0001);
				spot_factor = clamp((theta - outer_cos) / epsilon, 0.0, 1.0);
			}
		}

		vec3 half_vector = normalize(view_dir + light_dir);
		vec3 radiance = light.color_intensity.rgb * attenuation * light.color_intensity.a * spot_factor;
		float shadow = receives_shadows ? light_shadow(light, world_pos, normal, light_dir) : 1.0;
		float ndf = distribution_ggx(normal, half_vector, roughness);
		float geometry = geometry_smith(normal, view_dir, light_dir, roughness);
		vec3 fresnel = fresnel_schlick(max(dot(half_vector, view_dir), 0.0), f0);

		vec3 specular = ndf * geometry * fresnel /
			max(4.0 * max(dot(normal, view_dir), 0.0) * max(dot(normal, light_dir), 0.0), 0.001);
		vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / PI;
		outgoing += (diffuse + specular) * radiance * max(dot(normal, light_dir), 0.0) * shadow;
	}

	return outgoing;
}

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
	vec3 baked_indirect = albedo * texture(g_baked_light, uv).rgb;
	vec3 runtime_direct = evaluate_direct_lights(world_pos, normal, view_dir, albedo,
		metallic, roughness, receives_shadows);
	// Shadow visibility is applied inside runtime_direct only. A fully shadowed
	// mixed light must not erase the independent baked-indirect contribution.
	vec3 color = ambient + baked_indirect + runtime_direct;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));
	frag_color = vec4(color, 1.0);
}
