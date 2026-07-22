#version 330 core
in vec3 local_direction;
out vec4 frag_color;
uniform samplerCube environment_map;
uniform float roughness;
const float PI = 3.14159265359;
float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 hammersley(uint i, uint count) { return vec2(float(i) / float(count), radical_inverse(i)); }
float distribution_ggx(vec3 n, vec3 h, float r) {
    float a = r * r;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float denominator = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.000001);
}
vec3 importance_sample(vec2 xi, vec3 n, float r) {
    float a = r * r;
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    vec3 h = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
    return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}
void main() {
    vec3 n = normalize(local_direction);
    vec3 v = n;
    vec3 color = vec3(0.0);
    float weight = 0.0;
    const uint count = 256u;
    for (uint i = 0u; i < count; ++i) {
        vec3 h = importance_sample(hammersley(i, count), n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);
        float ndotl = max(dot(n, l), 0.0);
        if (ndotl > 0.0) {
            float ndoth = max(dot(n, h), 0.0);
            float hdv = max(dot(h, v), 0.0);
            float pdf = distribution_ggx(n, h, roughness) * ndoth / max(4.0 * hdv, 0.0001);
            float sample_solid_angle = 1.0 / max(float(count) * pdf, 0.0001);
            float texel_solid_angle = 4.0 * PI / (6.0 * 512.0 * 512.0);
            float mip = roughness <= 0.0001 ? 0.0 :
                max(0.0, 0.5 * log2(sample_solid_angle / texel_solid_angle));
            color += textureLod(environment_map, l, mip).rgb * ndotl;
            weight += ndotl;
        }
    }
    frag_color = vec4(color / max(weight, 0.0001), 1.0);
}
