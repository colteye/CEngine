#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec3 i_normal_pos;
layout(location = 2) in vec2 i_vertex_uv;

// Output data will be interpolated for each fragment.
out vec3 normal_pos;
out vec3 vertex_pos_world;
out vec2 uv;

// Values that stay constant for the whole mesh.
uniform mat4 model; // model matrix.
uniform mat4 view; // view matrix.
uniform mat4 projection; // projection matrix.

void main(){

	// Output position of the vertex, in clip space : MVP * position.
    gl_Position = projection* view * model * vec4(i_vertex_pos, 1);

    normal_pos = i_normal_pos;
    vertex_pos_world = vec3(model * vec4(i_vertex_pos, 1.0));

    // UV of the vertex. No special space for this one.
    uv = i_vertex_uv;
}