//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core
in vec3 direction;
out vec4 frag_color;
uniform samplerCube environment_map;
uniform float intensity;
uniform float rotation_radians;
void main() {
    float c = cos(rotation_radians), s = sin(rotation_radians);
    vec3 d = normalize(direction);
    d.xy = mat2(c, -s, s, c) * d.xy;
    vec3 color = texture(environment_map, d).rgb * intensity;
    frag_color = vec4(color, 1.0);
}
