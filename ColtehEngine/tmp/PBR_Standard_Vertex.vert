#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec3 normal_pos;
layout(location = 2) in vec2 vertex_uv;

// Output data will be interpolated for each fragment.
out vec2 uv;
out vec3 normal_pos_camera;
out vec3 light_dir_camera;
out vec3 view_dir_camera;
out float distance_to_light;

// Values that stay constant for the whole mesh.
uniform mat4 MVP; // model view projection matrix.

uniform mat4 M; // model matrix.
uniform mat4 V; // view matrix.
uniform mat4 P; // projection matrix.

uniform vec3 light_pos_world; // light world position.

void main(){

	// Output position of the vertex, in clip space : MVP * position.
    gl_Position =  MVP * vec4(vertex_pos, 1);

	// Camera space positions and directions.
	vec3 vertex_pos_camera = ( V * M * vec4(vertex_pos, 1)).xyz;
		 normal_pos_camera = ( V * M * vec4(normal_pos, 0)).xyz;
	
	vec3 light_pos_camera = ( V * vec4(light_pos_world, 1)).xyz;
		 view_dir_camera = vec3(0,0,0) - vertex_pos_camera;	
		 light_dir_camera = light_pos_camera + view_dir_camera;

	vec3 vertex_pos_world = (M * vec4(vertex_pos, 1)).xyz;
	distance_to_light = distance(vertex_pos_world, light_pos_world);
	
    // UV of the vertex. No special space for this one.
    uv = vertex_uv;
}