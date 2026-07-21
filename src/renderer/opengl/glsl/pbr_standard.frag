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
uniform sampler2D shadow_atlas;
uniform samplerCube point_shadow_maps[8];
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
    vec4 point_shadow_params[8];
    vec4 point_filter_params[8];
    vec4 shadow_counts;
};

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float NdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - NdotH, 5.0);
}

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

float AtlasShadow(mat4 light_matrix, vec4 rect, vec4 params, vec3 world_pos, vec3 N, vec3 L)
{
    vec4 clip_pos = light_matrix * vec4(world_pos, 1.0);
    vec3 projected = clip_pos.xyz / clip_pos.w;
    vec2 shadow_uv = projected.xy * 0.5 + 0.5;
    float current_depth = projected.z * 0.5 + 0.5;
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        current_depth < 0.0 || current_depth > 1.0) {
        return 1.0;
    }

    vec2 atlas_uv = rect.xy + shadow_uv * rect.zw;
    vec2 atlas_min = rect.xy;
    vec2 atlas_max = rect.xy + rect.zw;
    float bias = max(params.x * (1.0 - dot(N, L)), params.x * 0.25) + params.y * 0.0001;
    float atlas_texel = 1.0 / shadow_counts.w;
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 sample_uv = clamp(atlas_uv + vec2(x, y) * atlas_texel, atlas_min, atlas_max);
            float closest_depth = texture(shadow_atlas, sample_uv).r;
            lit += current_depth - bias <= closest_depth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

float SamplePointMap(int index, vec3 direction)
{
    if (index == 0) return texture(point_shadow_maps[0], direction).r;
    if (index == 1) return texture(point_shadow_maps[1], direction).r;
    if (index == 2) return texture(point_shadow_maps[2], direction).r;
    if (index == 3) return texture(point_shadow_maps[3], direction).r;
    if (index == 4) return texture(point_shadow_maps[4], direction).r;
    if (index == 5) return texture(point_shadow_maps[5], direction).r;
    if (index == 6) return texture(point_shadow_maps[6], direction).r;
    return texture(point_shadow_maps[7], direction).r;
}

float PointShadow(int index, vec3 world_pos, vec3 N, vec3 L)
{
    vec4 point_data = point_shadow_params[index];
    vec4 filter_data = point_filter_params[index];
    vec3 from_light = world_pos - point_data.xyz;
    float current_distance = length(from_light);
    float far_plane = point_data.w;
    if (far_plane <= 0.0 || current_distance >= far_plane) {
        return 1.0;
    }

    float bias = max(filter_data.x * (1.0 - dot(N, L)), filter_data.x * 0.25) + filter_data.y * 0.0001;
    float disk_radius = max(0.01, current_distance / far_plane * 0.08);
    vec3 offsets[7] = vec3[](
        vec3(0.0, 0.0, 0.0),
        vec3(1.0, 0.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 0.0, -1.0)
    );

    float lit = 0.0;
    for (int sample_index = 0; sample_index < 7; ++sample_index) {
        float closest_distance = SamplePointMap(index, from_light + offsets[sample_index] * disk_radius) * far_plane;
        lit += current_distance - bias <= closest_distance ? 1.0 : 0.0;
    }
    return lit / 7.0;
}

float DirectionalShadow(int cascade_start, vec3 world_pos, vec3 N, vec3 L)
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

    float shadow = AtlasShadow(cascade_shadow_matrices[selected], cascade_atlas_rects[selected],
        vec4(cascade_shadow_params[selected].y, cascade_shadow_params[selected].z, 0.0, 0.0),
        world_pos, N, L);
    float blend_range = cascade_shadow_params[selected].w;
    if (blend_range > 0.0 && selected + 1 < cascade_start + 4) {
        float split = cascade_shadow_params[selected].x;
        float blend = smoothstep(0.0, 1.0, clamp((view_depth - (split - blend_range)) / blend_range, 0.0, 1.0));
        float next_shadow = AtlasShadow(cascade_shadow_matrices[selected + 1], cascade_atlas_rects[selected + 1],
            vec4(cascade_shadow_params[selected + 1].y, cascade_shadow_params[selected + 1].z, 0.0, 0.0),
            world_pos, N, L);
        shadow = mix(shadow, next_shadow, blend);
    }
    return shadow;
}

float LightShadow(GpuLight light, vec3 world_pos, vec3 N, vec3 L)
{
    int index = int(light.params.z + 0.5);
    float shadow_type = light.params.w;
    if (index < 0 || abs(shadow_type - SHADOW_TYPE_NONE) < 0.5) {
        return 1.0;
    }
    if (abs(shadow_type - SHADOW_TYPE_SPOT) < 0.5) {
        return AtlasShadow(spot_shadow_matrices[index], spot_atlas_rects[index], spot_shadow_params[index],
            world_pos, N, L);
    }
    if (abs(shadow_type - SHADOW_TYPE_DIRECTIONAL) < 0.5) {
        return DirectionalShadow(index, world_pos, N, L);
    }
    if (abs(shadow_type - SHADOW_TYPE_POINT) < 0.5) {
        return PointShadow(index, world_pos, N, L);
    }
    return 1.0;
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

    vec3 albedo = albedo_sample.rgb;
	
    vec3 mra = texture(metallic_roughness_ao, uv).rgb * metallic_roughness_ao_factors;
    float metallic = mra.r;
    float roughness = mra.g;
    float ao = mra.b;

    if (render_mode == RENDER_MODE_UNLIT) {
        vec3 unlit_color = albedo / (albedo + vec3(1.0));
        frag_color = vec4(pow(unlit_color, vec3(1.0 / 2.2)), albedo_sample.a);
        return;
    }

    vec3 N = CalculateNormals();

    vec3 V = normalize(cam_pos_world - vertex_pos_world);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
    int light_count = min(int(light_info.x), MAX_DIRECT_LIGHTS);
    for(int i = 0; i < light_count; ++i) 
    {
        GpuLight light = lights[i];
        float light_type = light.params.x;
        float attenuation = 1.0;
        float spot_factor = 1.0;
        vec3 L = vec3(0.0);

        // calculate per-light radiance
        if (abs(light_type - LIGHT_TYPE_DIRECTIONAL) < 0.5)
        {
            L = normalize(-light.direction_spot.xyz);
        }
        else
        {
            vec3 light_delta = light.position_range.xyz - vertex_pos_world;
            float distance = length(light_delta);
            if (distance <= 0.0001)
            {
                continue;
            }

            L = light_delta / distance;
            attenuation = 1.0 / (distance * distance);

            float range = light.position_range.w;
            if (range > 0.0)
            {
                float range_factor = clamp(1.0 - distance / range, 0.0, 1.0);
                attenuation *= range_factor * range_factor;
            }

            if (abs(light_type - LIGHT_TYPE_SPOT) < 0.5)
            {
                float theta = dot(normalize(-L), normalize(light.direction_spot.xyz));
                float inner_cos = light.params.y;
                float outer_cos = light.direction_spot.w;
                float epsilon = max(inner_cos - outer_cos, 0.0001);
                spot_factor = clamp((theta - outer_cos) / epsilon, 0.0, 1.0);
            }
        }

        vec3 H = normalize(V + L);
        vec3 radiance = light.color_intensity.rgb * attenuation * 100.0 * light.color_intensity.a * spot_factor;        
        float shadow = receives_shadows ? LightShadow(light, vertex_pos_world, N, L) : 1.0;
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);        
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular     = numerator / max(denominator, 0.001);  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
    }   
  
    float sky_weight = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient_color = mix(ambient_ground_color, ambient_sky_color, sky_weight);
	vec3 ambient = ambient_enabled && !has_lightmap ?
		ambient_color * ambient_intensity * albedo * ao : vec3(0.0);
	vec3 baked_light = vec3(0.0);
	if (has_lightmap) {
		vec2 atlas_uv = lightmap_uv * lightmap_scale_offset.xy + lightmap_scale_offset.zw;
		vec4 rgbm = texture(lightmap, atlas_uv);
		baked_light = rgbm.rgb * (rgbm.a * lightmap_rgbm_range);
	}
    vec3 color = ambient + baked_light + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    frag_color = vec4(color, albedo_sample.a);
}  
