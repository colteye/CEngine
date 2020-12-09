#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;
layout(location = 2) in vec3 i_normal_pos;
layout(location = 3) in vec3 i_tangent_pos;
layout(location = 4) in vec3 i_bitangent_pos;

// Output data will be interpolated for each fragment.
out vec3 normal_pos_world;
out vec3 tangent_pos_world;
out vec3 bitangent_pos_world;

out vec3 vertex_pos_world;
out vec2 uv;

// Values that stay constant for the whole mesh.
uniform mat4 model; // model matrix.
uniform mat4 view; // view matrix.
uniform mat4 projection; // projection matrix.

void main(){

	// Output position of the vertex, in clip space : MVP * position.
    gl_Position = projection* view * model * vec4(i_vertex_pos, 1);

    vertex_pos_world = vec3(model * vec4(i_vertex_pos, 1.0));
	
	// Convert normal basis vectors into world space.
	normal_pos_world = vec3(model * vec4(i_normal_pos, 1.0));
	tangent_pos_world = vec3(model * vec4(i_tangent_pos, 1.0));
	bitangent_pos_world = vec3(model * vec4(i_bitangent_pos, 1.0));

    // UV of the vertex.
    uv = i_vertex_uv;

}