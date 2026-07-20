#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D g_albedo;
uniform sampler2D g_normal_roughness;
uniform sampler2D g_material;
uniform sampler2D g_depth;

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

vec3 evaluate_direct_lights(vec3 world_pos, vec3 normal, vec3 view_dir, vec3 albedo, float metallic, float roughness)
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
		vec3 radiance = light.color_intensity.rgb * attenuation * 100.0 * light.color_intensity.a * spot_factor;
		float ndf = distribution_ggx(normal, half_vector, roughness);
		float geometry = geometry_smith(normal, view_dir, light_dir, roughness);
		vec3 fresnel = fresnel_schlick(max(dot(half_vector, view_dir), 0.0), f0);

		vec3 specular = ndf * geometry * fresnel /
			max(4.0 * max(dot(normal, view_dir), 0.0) * max(dot(normal, light_dir), 0.0), 0.001);
		vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / PI;
		outgoing += (diffuse + specular) * radiance * max(dot(normal, light_dir), 0.0);
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
	vec3 world_pos = world_position_from_depth(depth);
	vec3 view_dir = normalize(cam_pos_world - world_pos);

	float sky_weight = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 ambient_color = mix(ambient_ground_color, ambient_sky_color, sky_weight);
	vec3 ambient = ambient_enabled ? ambient_color * ambient_intensity * albedo * ao : vec3(0.0);
	vec3 color = ambient + evaluate_direct_lights(world_pos, normal, view_dir, albedo, metallic, roughness);

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));
	frag_color = vec4(color, 1.0);
}
