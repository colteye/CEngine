#version 330 core

layout(location = 0) in vec2 corner;
layout(location = 1) in vec2 vertex_uv;
layout(location = 2) in vec3 particle_position;
layout(location = 3) in float particle_size;
layout(location = 4) in vec4 particle_color;

uniform mat4 view_projection;
uniform vec3 camera_right;
uniform vec3 camera_up;

out vec2 uv;
out vec4 color;

void main()
{
    vec3 world_position = particle_position +
                          (camera_right * corner.x + camera_up * corner.y) * particle_size;
    gl_Position = view_projection * vec4(world_position, 1.0);
    uv = vertex_uv;
    color = particle_color;
}
