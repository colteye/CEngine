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
layout(location = 5) in uvec4 i_joints;
layout(location = 6) in vec4 i_weights;

#include "skinning.glsl"

out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	mat4 skin = cengine_skin_matrix(i_joints, i_weights);
	gl_Position = projection * view * model * skin * vec4(i_vertex_pos, 1.0);
	uv = i_vertex_uv;
}
