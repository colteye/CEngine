//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core

layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;
layout(location = 2) in vec3 i_normal_pos;
layout(location = 3) in vec3 i_tangent_pos;
layout(location = 4) in vec2 i_lightmap_uv;
layout(location = 5) in uvec4 i_joints;
layout(location = 6) in vec4 i_weights;

#include "skinning.glsl"

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
	mat4 skin = cengine_skin_matrix(i_joints, i_weights);
	vec4 local_position = skin * vec4(i_vertex_pos, 1.0);
	gl_Position = projection * view * model * local_position;

	normal_pos_world = normalize(mat3(model * skin) * i_normal_pos);
	tangent_pos_world = normalize(mat3(model * skin) * i_tangent_pos);
	bitangent_pos_world = normalize(cross(normal_pos_world, tangent_pos_world));

	uv = i_vertex_uv;
	lightmap_uv = i_lightmap_uv;
}
