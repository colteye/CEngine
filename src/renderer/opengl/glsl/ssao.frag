//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#version 330 core

in vec2 uv;
layout(location = 0) out float frag_ao;

uniform sampler2D depth_tex;
uniform sampler2D normal_roughness_tex;
uniform mat4 projection;
uniform mat4 inverse_projection;
uniform mat4 view;
uniform float radius;
uniform float bias;
uniform float intensity;
uniform float contrast;

const int sample_count = 8;
const vec3 kernel[sample_count] = vec3[sample_count](
    vec3( 0.5381,  0.1856, 0.4319),
    vec3( 0.1379,  0.2486, 0.4430),
    vec3( 0.3371,  0.5679, 0.1057),
    vec3(-0.6999, -0.0451, 0.2019),
    vec3( 0.0689, -0.1598, 0.5547),
    vec3( 0.0560,  0.0069, 0.2843),
    vec3(-0.0146,  0.1402, 0.3762),
    vec3( 0.0100, -0.1924, 0.3344)
);

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 view_position_from_depth(vec2 tex_coord)
{
    float depth = texture(depth_tex, tex_coord).r;
    vec4 clip_pos = vec4(tex_coord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view_pos = inverse_projection * clip_pos;
    return view_pos.xyz / view_pos.w;
}

vec3 view_normal_from_gbuffer(vec2 tex_coord)
{
    vec3 world_normal = normalize(texture(normal_roughness_tex, tex_coord).rgb * 2.0 - 1.0);
    return normalize(mat3(view) * world_normal);
}

float interleaved_gradient_noise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

mat3 tangent_frame(vec3 normal, float rotation)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    float c = cos(rotation);
    float s = sin(rotation);
    return mat3(tangent * c + bitangent * s, bitangent * c - tangent * s, normal);
}

float sample_ao(vec3 view_pos, mat3 frame, int index)
{
    float scale = float(index + 1) / float(sample_count);
    vec3 sample_pos = view_pos + frame * kernel[index] * radius * mix(0.2, 1.0, scale * scale);

    vec4 clip_pos = projection * vec4(sample_pos, 1.0);
    vec2 sample_uv = (clip_pos.xy / clip_pos.w) * 0.5 + 0.5;

    if (sample_uv.x <= 0.0 || sample_uv.x >= 1.0 || sample_uv.y <= 0.0 || sample_uv.y >= 1.0) {
        return 0.0;
    }

    float blocker_depth = texture(depth_tex, sample_uv).r;
    if (blocker_depth >= 1.0) {
        return 0.0;
    }

    vec3 blocker_pos = view_position_from_depth(sample_uv);
    float depth_delta = abs(view_pos.z - blocker_pos.z);
    if (depth_delta > radius * 2.0) {
        return 0.0;
    }

    float range_weight = smoothstep(0.0, 1.0, radius / max(depth_delta, 0.0001));
    float occluded = blocker_pos.z >= sample_pos.z + bias ? 1.0 : 0.0;

    return occluded * range_weight;
}

float raw_ao(vec2 tex_coord)
{
    float depth = texture(depth_tex, tex_coord).r;
    if (depth >= 1.0) {
        return 1.0;
    }

    vec3 view_pos = view_position_from_depth(tex_coord);
    vec3 normal = view_normal_from_gbuffer(tex_coord);
    float noise = interleaved_gradient_noise(gl_FragCoord.xy);
    mat3 frame = tangent_frame(normal, noise * 6.28318530718);

    float occlusion = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        occlusion += sample_ao(view_pos, frame, i);
    }

    float ao = 1.0 - intensity * occlusion / float(sample_count);
    return pow(saturate(ao), contrast);
}

void main()
{
	frag_ao = raw_ao(uv);
}
