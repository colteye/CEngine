#version 330 core

layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;
layout(location = 2) in vec3 i_normal_pos;
layout(location = 3) in vec3 i_tangent_pos;
layout(location = 4) in vec2 i_lightmap_uv;

out vec2 uv;
out vec3 normal_pos_world;
out vec3 tangent_pos_world;
out vec3 bitangent_pos_world;
out vec2 lightmap_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(i_vertex_pos, 1.0);

	normal_pos_world = normalize(mat3(model) * i_normal_pos);
	tangent_pos_world = normalize(mat3(model) * i_tangent_pos);
	bitangent_pos_world = normalize(cross(tangent_pos_world, normal_pos_world));

	uv = i_vertex_uv;
	lightmap_uv = i_lightmap_uv;
}
