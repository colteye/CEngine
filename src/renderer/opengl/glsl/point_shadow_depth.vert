#version 330 core

layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;

out vec2 uv;
out vec3 world_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	vec4 world = model * vec4(i_vertex_pos, 1.0);
	world_pos = world.xyz;
	uv = i_vertex_uv;
	gl_Position = projection * view * world;
}
