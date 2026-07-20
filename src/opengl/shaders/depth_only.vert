#version 330 core

layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;

out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(i_vertex_pos, 1.0);
	uv = i_vertex_uv;
}
