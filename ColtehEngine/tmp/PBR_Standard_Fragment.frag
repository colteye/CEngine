#version 330 core

// Interpolated values from the vertex shaders
in vec2 uv;
in vec3 normal_pos_camera;
in vec3 light_dir_camera;
in vec3 view_dir_camera;
in float distance_to_light;

// Ouput data
out vec3 color;

// Values that stay constant for the whole mesh.
uniform vec3 light_color;
uniform float light_power;
uniform sampler2D albedo;

void main(){

	// Diffuse pass.
	vec3 N = normalize(normal_pos_camera);
	vec3 L = normalize(light_dir_camera);
	
	// Use lambertian diffuse.
	float cos_theta = clamp( dot(N, L), 0,1 );

	vec3 tex_color = texture(albedo, uv).rgb;
	vec3 ambient = vec3(0.1, 0.1, 0.1) * tex_color;
	
	color = ambient + (tex_color * light_color * light_power * cos_theta);
	
	// Specular pass.
	vec3 E = normalize(view_dir_camera);
	vec3 R = reflect(-L, N);
	float cos_alpha = clamp( dot(E, R), 0,1 );

	color = color + (vec3(1, 1, 1) * light_color * light_power * pow(cos_alpha,5));
	
	// Squared falloff for both passes.
	color = color / (distance_to_light * distance_to_light);
}