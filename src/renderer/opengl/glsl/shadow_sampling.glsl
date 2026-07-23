float atlas_shadow(mat4 light_matrix, vec4 rect, vec4 params,
	vec3 world_pos, vec3 normal, vec3 light_dir)
{
	vec3 receiver_pos = world_pos + normal * params.y;
	vec4 clip_pos = light_matrix * vec4(receiver_pos, 1.0);
	vec3 projected = clip_pos.xyz / clip_pos.w;
	vec2 shadow_uv = projected.xy * 0.5 + 0.5;
	float current_depth = projected.z * 0.5 + 0.5;
	if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
		shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
		current_depth < 0.0 || current_depth > 1.0)
		return 1.0;

	float atlas_texel = 1.0 / shadow_counts.w;
	vec2 atlas_uv = rect.xy + shadow_uv * rect.zw;
	vec2 atlas_min = rect.xy + vec2(atlas_texel * 1.5);
	vec2 atlas_max = rect.xy + rect.zw - vec2(atlas_texel * 1.5);
	float bias = max(params.x *
		(1.0 - clamp(dot(normal, light_dir), 0.0, 1.0)),
		params.x * 0.25);

	float lit = 0.0;
	for (int y = -1; y <= 1; ++y)
		for (int x = -1; x <= 1; ++x)
		{
			vec2 sample_uv = clamp(atlas_uv +
				vec2(float(x), float(y)) * atlas_texel,
				atlas_min, atlas_max);
			lit += texture(
				shadow_atlas, vec3(sample_uv, current_depth - bias));
		}
	return lit / 9.0;
}

float sample_point_shadow(
	int index, vec3 direction, float reference_depth)
{
	vec4 sample_coord = vec4(direction, reference_depth);
	if (index == 0) return texture(point_shadow_maps[0], sample_coord);
	if (index == 1) return texture(point_shadow_maps[1], sample_coord);
	if (index == 2) return texture(point_shadow_maps[2], sample_coord);
	if (index == 3) return texture(point_shadow_maps[3], sample_coord);
	return 1.0;
}

float point_shadow(
	int index, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	vec4 point_data = point_shadow_params[index];
	vec4 filter_data = point_filter_params[index];
	vec3 receiver_pos = world_pos + normal * filter_data.y;
	vec3 from_light = receiver_pos - point_data.xyz;
	float current_distance = length(from_light);
	float far_plane = point_data.w;
	if (far_plane <= 0.0 || current_distance >= far_plane)
		return 1.0;

	float bias = max(filter_data.x *
		(1.0 - clamp(dot(normal, light_dir), 0.0, 1.0)),
		filter_data.x * 0.25);
	float reference_depth =
		clamp(current_distance / far_plane - bias, 0.0, 1.0);
	float disk_radius = 3.0 / max(filter_data.z, 64.0);
	vec3 offsets[4] = vec3[](
		vec3(1.0, 1.0, 1.0),
		vec3(1.0, -1.0, -1.0),
		vec3(-1.0, 1.0, -1.0),
		vec3(-1.0, -1.0, 1.0)
	);

	float lit = 0.0;
	vec3 direction = from_light / current_distance;
	for (int sample_index = 0; sample_index < 4; ++sample_index)
		lit += sample_point_shadow(index,
			normalize(direction + offsets[sample_index] * disk_radius),
			reference_depth);
	return lit * 0.25;
}

float directional_shadow(
	int cascade_start, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	float view_depth = -(view * vec4(world_pos, 1.0)).z;
	int selected = -1;
	for (int cascade = 0; cascade < 4; ++cascade)
	{
		int index = cascade_start + cascade;
		if (view_depth <= cascade_shadow_params[index].x)
		{
			selected = index;
			break;
		}
	}
	if (selected < 0) return 1.0;

	float shadow = atlas_shadow(
		cascade_shadow_matrices[selected],
		cascade_atlas_rects[selected],
		vec4(cascade_shadow_params[selected].yz, 0.0, 0.0),
		world_pos, normal, light_dir);
	float blend_range = cascade_shadow_params[selected].w;
	if (blend_range > 0.0 && selected + 1 < cascade_start + 4)
	{
		float split = cascade_shadow_params[selected].x;
		float blend = smoothstep(0.0, 1.0, clamp(
			(view_depth - (split - blend_range)) / blend_range,
			0.0, 1.0));
		float next_shadow = atlas_shadow(
			cascade_shadow_matrices[selected + 1],
			cascade_atlas_rects[selected + 1],
			vec4(cascade_shadow_params[selected + 1].yz, 0.0, 0.0),
			world_pos, normal, light_dir);
		shadow = mix(shadow, next_shadow, blend);
	}
	return shadow;
}

float light_shadow(
	GpuLight light, vec3 world_pos, vec3 normal, vec3 light_dir)
{
	int index = int(light.params.z + 0.5);
	float shadow_type = light.params.w;
	if (index < 0 || abs(shadow_type - SHADOW_TYPE_NONE) < 0.5)
		return 1.0;
	if (abs(shadow_type - SHADOW_TYPE_SPOT) < 0.5)
		return atlas_shadow(
			spot_shadow_matrices[index], spot_atlas_rects[index],
			spot_shadow_params[index], world_pos, normal, light_dir);
	if (abs(shadow_type - SHADOW_TYPE_DIRECTIONAL) < 0.5)
		return directional_shadow(
			index, world_pos, normal, light_dir);
	if (abs(shadow_type - SHADOW_TYPE_POINT) < 0.5)
		return point_shadow(index, world_pos, normal, light_dir);
	return 1.0;
}
