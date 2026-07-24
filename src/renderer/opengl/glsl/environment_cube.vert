//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core
layout(location = 0) in vec3 position;
out vec3 local_direction;
uniform mat4 projection;
uniform mat4 view;
void main() {
    local_direction = position;
    gl_Position = projection * view * vec4(position, 1.0);
}
