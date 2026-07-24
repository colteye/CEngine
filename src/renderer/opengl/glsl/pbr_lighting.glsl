//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

float distribution_ggx(
	vec3 normal, vec3 half_vector, float roughness)
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

float geometry_smith(
	vec3 normal, vec3 view_dir, vec3 light_dir, float roughness)
{
	return geometry_schlick_ggx(
		max(dot(normal, view_dir), 0.0), roughness) *
		geometry_schlick_ggx(
			max(dot(normal, light_dir), 0.0), roughness);
}

vec3 fresnel_schlick(float hdotv, vec3 f0)
{
	return f0 + (1.0 - f0) * pow(1.0 - hdotv, 5.0);
}

vec3 rotate_environment(vec3 direction)
{
	float c = cos(ibl_rotation_radians);
	float s = sin(ibl_rotation_radians);
	direction.xy = mat2(c, -s, s, c) * direction.xy;
	return direction;
}

vec3 fresnel_schlick_roughness(
	float ndotv, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) *
		pow(1.0 - ndotv, 5.0);
}

vec2 environment_brdf(float ndotv, float roughness)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 =
		min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
	return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 apply_height_fog(vec3 color, vec3 world_pos)
{
	if (!fog_enabled) return color;
	vec3 delta = world_pos - cam_pos_world;
	float distance_to_surface = length(delta);
	if (distance_to_surface <= fog_start_distance ||
		(fog_cutoff_distance > 0.0 &&
			distance_to_surface > fog_cutoff_distance))
		return color;
	vec3 ray = delta / max(distance_to_surface, 0.0001);
	float distance_in_fog =
		distance_to_surface - fog_start_distance;
	float start_height =
		cam_pos_world.z + ray.z * fog_start_distance;
	float start_density = fog_density * exp(clamp(
		-fog_height_falloff * (start_height - fog_base_height),
		-80.0, 80.0));
	float slope = fog_height_falloff * ray.z;
	float optical_depth = abs(slope) < 0.00001 ?
		start_density * distance_in_fog :
		start_density * (1.0 - exp(-slope * distance_in_fog)) /
			slope;
	float amount = min(
		1.0 - exp(-max(optical_depth, 0.0)), fog_max_opacity);
	return mix(color, fog_inscattering_color, amount);
}

#include "shadow_sampling.glsl"

vec3 evaluate_direct_lights(
	vec3 world_pos, vec3 normal, vec3 view_dir, vec3 albedo,
	float metallic, float roughness, bool receives_shadows)
{
	vec3 f0 = mix(vec3(0.04), albedo, metallic);
	vec3 outgoing = vec3(0.0);
	int light_count = min(int(light_info.x), MAX_DIRECT_LIGHTS);

	for (int i = 0; i < light_count; ++i)
	{
		GpuLight light = lights[i];
		float light_type = light.params.x;
		float attenuation = 1.0;
		float spot_factor = 1.0;
		vec3 light_dir = vec3(0.0);

		if (abs(light_type - LIGHT_TYPE_DIRECTIONAL) < 0.5)
			light_dir = normalize(-light.direction_spot.xyz);
		else
		{
			vec3 light_delta =
				light.position_range.xyz - world_pos;
			float distance = length(light_delta);
			if (distance <= 0.0001) continue;

			light_dir = light_delta / distance;
			attenuation = 1.0 / (distance * distance);
			float range = light.position_range.w;
			if (range > 0.0)
			{
				float range_factor =
					clamp(1.0 - distance / range, 0.0, 1.0);
				attenuation *= range_factor * range_factor;
			}

			if (abs(light_type - LIGHT_TYPE_SPOT) < 0.5)
			{
				float theta = dot(
					normalize(-light_dir),
					normalize(light.direction_spot.xyz));
				float epsilon = max(
					light.params.y - light.direction_spot.w,
					0.0001);
				spot_factor = clamp(
					(theta - light.direction_spot.w) / epsilon,
					0.0, 1.0);
			}
		}

		vec3 half_vector = normalize(view_dir + light_dir);
		vec3 radiance = light.color_intensity.rgb * attenuation *
			light.color_intensity.a * spot_factor;
		float shadow = receives_shadows ?
			light_shadow(
				light, world_pos, normal, light_dir) : 1.0;
		float ndf =
			distribution_ggx(normal, half_vector, roughness);
		float geometry =
			geometry_smith(normal, view_dir, light_dir, roughness);
		vec3 fresnel = fresnel_schlick(
			max(dot(half_vector, view_dir), 0.0), f0);
		vec3 specular = ndf * geometry * fresnel / max(
			4.0 * max(dot(normal, view_dir), 0.0) *
				max(dot(normal, light_dir), 0.0),
			0.001);
		vec3 diffuse = (vec3(1.0) - fresnel) *
			(1.0 - metallic) * albedo / PI;
		outgoing += (diffuse + specular) * radiance *
			max(dot(normal, light_dir), 0.0) * shadow;
	}
	return outgoing;
}
