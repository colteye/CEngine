#version 330 core
out vec4 frag_color;

// Data from vertex shader.
in vec2 uv;
in vec3 vertex_pos_world;
in vec3 normal_pos_world;
in vec3 tangent_pos_world;
in vec3 bitangent_pos_world;

// Material parameters.
uniform sampler2D albedo;
uniform sampler2D normal;
uniform sampler2D metallic_roughness_ao;

uniform vec3 cam_pos_world;
uniform vec4 base_color_factor;
uniform float alpha_cutoff;
uniform int render_mode;
uniform vec3 ambient_sky_color;
uniform vec3 ambient_ground_color;
uniform float ambient_intensity;
uniform bool ambient_enabled;

const int MAX_DIRECT_LIGHTS = 64;
const float LIGHT_TYPE_DIRECTIONAL = 0.0;
const float LIGHT_TYPE_POINT = 1.0;
const float LIGHT_TYPE_SPOT = 2.0;
const int RENDER_MODE_OPAQUE_DEFERRED = 0;
const int RENDER_MODE_OPAQUE_FORWARD = 1;
const int RENDER_MODE_ALPHA_CLIP = 2;
const int RENDER_MODE_ALPHA_HASH_DITHER = 3;
const int RENDER_MODE_TRANSPARENT_BLEND = 4;
const int RENDER_MODE_UNLIT = 5;

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
	
    float metallic = texture(metallic_roughness_ao, uv).r;
    float roughness = texture(metallic_roughness_ao, uv).g;
    float ao = texture(metallic_roughness_ao, uv).b;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
    }   
  
    float sky_weight = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient_color = mix(ambient_ground_color, ambient_sky_color, sky_weight);
    vec3 ambient = ambient_enabled ? ambient_color * ambient_intensity * albedo * ao : vec3(0.0);
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    frag_color = vec4(color, albedo_sample.a);
}  
