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

// Lights.
uniform vec3 light_positions[4];
uniform vec3 light_colors[4];
uniform float light_powers[4];
uniform vec3 cam_pos_world;

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
	normalize(cross(tangent_pos_world, normal_pos_world)), 
	normalize(normal_pos_world));
	
	// Convert normal from tangent space int world space.
    return normalize(TBN * normal_tex);
}

void main()
{		
    vec3 albedo = texture(albedo, uv).rgb;
	
    float metallic = texture(metallic_roughness_ao, uv).r;
    float roughness = texture(metallic_roughness_ao, uv).g;
    float ao = texture(metallic_roughness_ao, uv).b;

    vec3 N = CalculateNormals();

    vec3 V = normalize(cam_pos_world - vertex_pos_world);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i) 
    {
        // calculate per-light radiance
        vec3 L = normalize(light_positions[i] - vertex_pos_world);
        vec3 H = normalize(V + L);
        float distance    = length(light_positions[i] - vertex_pos_world);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = light_colors[i] * attenuation * 100 * light_powers[i];        
        
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
  
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    frag_color = vec4(color, 1.0);
}  