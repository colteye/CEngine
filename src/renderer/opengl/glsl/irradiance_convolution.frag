//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core
in vec3 local_direction;
out vec4 frag_color;
uniform samplerCube environment_map;
const float PI = 3.14159265359;
void main() {
    vec3 n = normalize(local_direction);
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, n));
    up = cross(n, right);
    vec3 irradiance = vec3(0.0);
    float samples = 0.0;
    const float delta = 0.08;
    for (float phi = 0.0; phi < 2.0 * PI; phi += delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta) {
            vec3 tangent = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sample_direction = tangent.x * right + tangent.y * up + tangent.z * n;
            irradiance += texture(environment_map, sample_direction).rgb * cos(theta) * sin(theta);
            samples += 1.0;
        }
    }
    frag_color = vec4(PI * irradiance / max(samples, 1.0), 1.0);
}
