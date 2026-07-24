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
out vec4 frag_color;

uniform sampler2D source_texture;
uniform sampler2D depth_texture;
uniform bool bloom_enabled;
uniform float bloom_threshold;
uniform float bloom_intensity;
uniform bool tone_mapping_enabled;
uniform float exposure;
uniform float contrast;
uniform float saturation;
uniform bool depth_of_field_enabled;
uniform float focus_distance;
uniform float focus_range;
uniform float depth_of_field_strength;
uniform bool sun_lens_flare_enabled;
uniform float sun_lens_flare_intensity;
uniform float sun_disc_size;
uniform float sun_disc_softness;
// xy is clip-space sun position; z is clip w. A negative w means it is behind us.
uniform vec3 sun_clip;
uniform float near_clip;
uniform float far_clip;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Khronos PBR Neutral maps linear Rec. 709 HDR to linear Rec. 709 display
// values while preserving base-color appearance and avoiding hue skews.
vec3 tone_map_pbr_neutral(vec3 color)
{
    color = max(color, vec3(0.0));
    const float start_compression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float minimum_channel = min(color.r, min(color.g, color.b));
    float offset = minimum_channel < 0.08
        ? minimum_channel - 6.25 * minimum_channel * minimum_channel
        : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression)
        return color;
    float compression_range = 1.0 - start_compression;
    float new_peak = 1.0 - compression_range * compression_range /
        (peak + compression_range - start_compression);
    color *= new_peak / peak;
    float highlight_desaturation =
        1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
    return mix(color, vec3(new_peak), highlight_desaturation);
}

float linear_depth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * near_clip * far_clip) / (far_clip + near_clip - z * (far_clip - near_clip));
}

vec3 sample_scene(vec2 sample_uv, float lod)
{
    return textureLod(source_texture, clamp(sample_uv, vec2(0.001), vec2(0.999)), lod).rgb;
}

vec3 apply_depth_of_field(float coc)
{
    if (coc <= 0.001) return sample_scene(uv, 0.0);
    vec2 texel = 1.0 / vec2(textureSize(source_texture, 0));
    float radius = 14.0 * coc;
    vec3 result = sample_scene(uv, 0.0) * 0.25;
    const vec2 taps[8] = vec2[8](
        vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
        vec2(0.707, 0.707), vec2(-0.707, 0.707), vec2(0.707, -0.707), vec2(-0.707, -0.707));
    for (int index = 0; index < 8; ++index)
        result += sample_scene(uv + taps[index] * texel * radius, 0.0) * 0.09375;
    return result;
}

vec3 apply_bloom(vec3 color)
{
    if (!bloom_enabled) return color;
    // Threshold every tap before averaging it. Thresholding an averaged mip
    // loses the small specular highlights that should produce bloom.
    vec2 texel = 1.0 / vec2(textureSize(source_texture, 0));
    vec3 glow = vec3(0.0);
    float weight = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float tap_weight = 1.0 / (1.0 + float(x * x + y * y));
            vec3 sample_color = sample_scene(uv + vec2(x, y) * texel * 3.0, 0.0);
            float brightness = luminance(sample_color);
            glow += sample_color * max(brightness - bloom_threshold, 0.0) /
                max(brightness, 0.0001) * tap_weight;
            weight += tap_weight;
        }
    }
    return color + glow / weight * bloom_intensity;
}

vec3 apply_sun_lens_flare(vec3 color)
{
    if (!sun_lens_flare_enabled || sun_clip.z <= 0.0) return color;
    vec2 sun_uv = sun_clip.xy / sun_clip.z * 0.5 + 0.5;
    // A lens still catches sunlight just beyond the frame. Fade over 18% of
    // the viewport rather than abruptly popping the effect off.
    vec2 outside = max(max(-sun_uv, sun_uv - vec2(1.0)), vec2(0.0));
    float edge_fade = 1.0 - smoothstep(0.0, 0.18, max(outside.x, outside.y));
    if (edge_fade <= 0.0) return color;

    // Sky-depth test makes the effect disappear behind walls, pillars, and terrain.
    float visible = smoothstep(0.985, 1.0,
        texture(depth_texture, clamp(sun_uv, vec2(0.001), vec2(0.999))).r) * edge_fade;
    vec2 axis = vec2(0.5) - sun_uv;
    float distance_to_sun = length(uv - sun_uv);
    // A small warm disc with limb darkening reads as a distant sun, unlike a
    // large flat sprite. The surrounding glow rolls off with camera exposure.
    float disc = 1.0 - smoothstep(sun_disc_size, sun_disc_size + sun_disc_softness, distance_to_sun);
    float disc_limb = sqrt(max(0.0, 1.0 - pow(distance_to_sun / max(sun_disc_size, 0.0001), 2.0)));
    float glow = exp(-distance_to_sun * 95.0) * 0.18;
    float halo = exp(-abs(distance_to_sun - 0.018) * 220.0) * 0.018;
    vec3 flare = vec3(1.0, 0.80, 0.56) * (disc * (0.75 + 0.25 * disc_limb) + glow + halo);
    // Two faint internal reflections are enough to convey glass optics.
    for (int index = 1; index <= 2; ++index) {
        float factor = float(index) * 0.55;
        vec2 ghost = sun_uv + axis * factor;
        float ghost_disc = pow(max(0.0, 1.0 - length(uv - ghost) * (22.0 + float(index) * 5.0)), 3.0);
        vec3 tint = mix(vec3(0.55, 0.72, 1.0), vec3(1.0, 0.62, 0.32), float(index) / 2.0);
        flare += tint * ghost_disc * 0.025;
    }
    return color + flare * visible * sun_lens_flare_intensity;
}

void main()
{
    float depth = texture(depth_texture, uv).r;
    float coc = 0.0;
    if (depth_of_field_enabled) {
        // Depth is 1 for the sky. Treat it as distant geometry so a near
        // focus visibly softens the skybox and silhouettes at the horizon.
        float subject_distance = depth >= 0.99999 ? far_clip : linear_depth(depth);
        coc = clamp(abs(subject_distance - focus_distance) / max(focus_range, 0.001), 0.0, 1.0)
            * min(depth_of_field_strength, 2.0);
    }
    vec3 color = apply_depth_of_field(coc);
    color = apply_bloom(color);
    color = apply_sun_lens_flare(color);
    if (tone_mapping_enabled) {
        color *= max(exposure, 0.0);
        color = tone_map_pbr_neutral(color);
        color = (color - 0.5) * contrast + 0.5;
        float luma = luminance(color);
        color = mix(vec3(luma), color, saturation);
    }
    frag_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
