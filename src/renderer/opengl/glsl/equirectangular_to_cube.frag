#version 330 core
in vec3 local_direction;
out vec4 frag_color;
uniform sampler2D panorama;
const float PI = 3.14159265359;
void main() {
    vec3 d = normalize(local_direction);
    vec2 uv = vec2(atan(d.y, d.x) / (2.0 * PI) + 0.5,
                   asin(clamp(d.z, -1.0, 1.0)) / PI + 0.5);
    frag_color = vec4(texture(panorama, uv).rgb, 1.0);
}
