const int MAX_ENVIRONMENT_PROBES = 2;

uniform int environment_probe_count;
uniform mat4 environment_probe_world_to_local[MAX_ENVIRONMENT_PROBES];
uniform mat4 environment_probe_local_to_world[MAX_ENVIRONMENT_PROBES];
uniform vec4 environment_probe_position_intensity[MAX_ENVIRONMENT_PROBES];
uniform vec4 environment_probe_extents_blend[MAX_ENVIRONMENT_PROBES];
uniform samplerCube environment_probe_irradiance_0;
uniform samplerCube environment_probe_prefiltered_0;
uniform samplerCube environment_probe_irradiance_1;
uniform samplerCube environment_probe_prefiltered_1;

float environment_probe_weight(int index, vec3 world_pos)
{
	vec3 local_pos = (environment_probe_world_to_local[index] *
		vec4(world_pos, 1.0)).xyz;
	vec3 absolute_pos = abs(local_pos);
	if (any(greaterThan(absolute_pos, vec3(1.0))))
		return 0.0;

	vec3 extents = environment_probe_extents_blend[index].xyz;
	float edge_distance = min(min(
		(1.0 - absolute_pos.x) * extents.x,
		(1.0 - absolute_pos.y) * extents.y),
		(1.0 - absolute_pos.z) * extents.z);
	float blend_distance = environment_probe_extents_blend[index].w;
	return blend_distance <= 0.0001 ? 1.0 :
		smoothstep(0.0, blend_distance, edge_distance);
}

vec3 environment_probe_reflection(
	int index, vec3 world_pos, vec3 world_reflection)
{
	vec3 local_pos = (environment_probe_world_to_local[index] *
		vec4(world_pos, 1.0)).xyz;
	vec3 local_ray = mat3(environment_probe_world_to_local[index]) *
		world_reflection;
	vec3 target = sign(local_ray);
	vec3 distance_to_plane = vec3(1e20);
	if (abs(local_ray.x) > 0.00001)
		distance_to_plane.x = (target.x - local_pos.x) / local_ray.x;
	if (abs(local_ray.y) > 0.00001)
		distance_to_plane.y = (target.y - local_pos.y) / local_ray.y;
	if (abs(local_ray.z) > 0.00001)
		distance_to_plane.z = (target.z - local_pos.z) / local_ray.z;
	float distance = min(distance_to_plane.x,
		min(distance_to_plane.y, distance_to_plane.z));
	vec3 local_hit = local_pos + local_ray * max(distance, 0.0);
	vec3 world_hit = (environment_probe_local_to_world[index] *
		vec4(local_hit, 1.0)).xyz;
	return normalize(world_hit -
		environment_probe_position_intensity[index].xyz);
}

vec3 sample_environment_probe_irradiance(int index, vec3 direction)
{
	return index == 0 ?
		texture(environment_probe_irradiance_0, direction).rgb :
		texture(environment_probe_irradiance_1, direction).rgb;
}

vec3 sample_environment_probe_prefiltered(
	int index, vec3 direction, float roughness)
{
	return index == 0 ?
		textureLod(environment_probe_prefiltered_0,
			direction, roughness * 4.0).rgb :
		textureLod(environment_probe_prefiltered_1,
			direction, roughness * 4.0).rgb;
}

vec3 evaluate_environment_probes(
	vec3 world_pos, vec3 normal, vec3 view_dir, vec3 albedo,
	float metallic, float roughness, float ao, out float coverage)
{
	float weights[MAX_ENVIRONMENT_PROBES];
	float weight_sum = 0.0;
	for (int index = 0; index < MAX_ENVIRONMENT_PROBES; ++index)
	{
		weights[index] = index < environment_probe_count ?
			environment_probe_weight(index, world_pos) : 0.0;
		weight_sum += weights[index];
	}
	coverage = clamp(weight_sum, 0.0, 1.0);
	if (weight_sum <= 0.0001)
		return vec3(0.0);

	float ndotv = max(dot(normal, view_dir), 0.0);
	vec3 f0 = mix(vec3(0.04), albedo, metallic);
	vec3 f = fresnel_schlick_roughness(ndotv, f0, roughness);
	vec3 kd = (vec3(1.0) - f) * (1.0 - metallic);
	vec2 brdf = environment_brdf(ndotv, roughness);
	vec3 world_reflection = reflect(-view_dir, normal);
	vec3 result = vec3(0.0);
	for (int index = 0; index < MAX_ENVIRONMENT_PROBES; ++index)
	{
		if (weights[index] <= 0.0)
			continue;
		vec3 diffuse = sample_environment_probe_irradiance(
			index, normal) * albedo;
		vec3 reflection = environment_probe_reflection(
			index, world_pos, world_reflection);
		vec3 prefiltered = sample_environment_probe_prefiltered(
			index, reflection, roughness);
		vec3 specular = prefiltered * (f * brdf.x + brdf.y);
		float normalized_weight = weights[index] /
			max(weight_sum, 1.0);
		result += (kd * diffuse * ao + specular * ao) *
			normalized_weight *
			environment_probe_position_intensity[index].w;
	}
	return result;
}
