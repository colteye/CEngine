#version 330 core
layout(location = 0) in vec3 position;
out vec3 direction;
uniform mat4 view;
uniform mat4 projection;
void main() {
    direction = position;
    vec4 clip = projection * mat4(mat3(view)) * vec4(position, 1.0);
    gl_Position = clip.xyww;
}
