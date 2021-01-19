#version 330 core

in vec2 uv;
layout(location = 0) out vec3 frag_color;

// Material parameters.
uniform sampler2D render_tex;
uniform sampler2D depth_tex;
uniform sampler2D random_tex;

const float total_strength = 1;
const float base = 0.1;

const float area = 0.001;
const float falloff = 0.0001;
  
const float radius = 0.03;

const float near = 0.1; 
const float far = 60; 


const int samples = 16;
const vec3 sample_sphere[samples] = {
  vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
  vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
  vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
  vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
  vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
  vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
  vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
  vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271)
};

vec3 normal_from_depth(float depth, vec2 uv) {
  
  vec2 offset1 = vec2(0.0,0.001);
  vec2 offset2 = vec2(0.001,0.0);
  
  float depth1 = texture(depth_tex, (uv + offset1)).r;
  float depth2 = texture(depth_tex, (uv + offset2)).r;
  
  vec3 p1 = vec3(offset1, depth1 - depth);
  vec3 p2 = vec3(offset2, depth2 - depth);
  
  vec3 normal = cross(p1, p2);
  normal.z = - normal.z;
  
  return normalize(normal);
}

float linearize_depth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near) / (far + near - z * (far - near));	
}

void main()
{
  vec3 random = normalize(texture(random_tex, uv * 8.0).rgb);
  
  float depth = texture(depth_tex, uv).r;
  
  float alt_depth = linearize_depth(depth);
 
  vec3 position = vec3(uv, depth);
  vec3 normal = normal_from_depth(depth, uv);
  
  float radius_depth = radius / depth;
  float occlusion = 0.0;
  for(int i=0; i < samples; i++) {
  
    vec3 ray = reflect(sample_sphere[i], random) * radius_depth;
    vec3 hemi_ray = position + sign(dot(ray , normal)) * ray;
    
	vec2 sat_hemi = clamp(hemi_ray.xy,0.0,1.0);
	
    float occ_depth = texture(depth_tex, sat_hemi).r;
    float difference = depth - occ_depth;

    occlusion += step(falloff, difference) * (1.0-smoothstep(falloff, area, difference));
  }
  
  float ao = 1.0 - total_strength * occlusion * (1.0 / samples);
  
  float sat_ao_base = clamp(ao + base, 0.0, 1.0);
  
  //frag_color = vec3(alt_depth, alt_depth, alt_depth);
  
  //frag_color = texture(depth_tex, uv).rgb;
  
  //frag_color = vec3(sat_ao_base, sat_ao_base, sat_ao_base)  + (alt_depth);
  
  //frag_color = normal;
  
 frag_color = texture(render_tex, uv).rgb * (vec3(sat_ao_base, sat_ao_base, sat_ao_base) + (alt_depth));
  
}