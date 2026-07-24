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

// Compact shader fit of Blender 5.2's AgX Base response with no creative look.
// Inputs and outputs are linear Rec. 709; the framebuffer performs the final
// sRGB transfer. The Chebyshev fit was sampled through Blender's OCIO display
// processor and stays within 0.005 display-code value over the fitted range.
vec3 agx_base_curve(vec3 value)
{
    const float coefficients[13] = float[13](
        0.432058687447, 0.558356010708, 0.093994897645,
        -0.068662775093, -0.037143879553, 0.012049449491,
        0.015065601865, -0.000492445341, -0.006416393403,
        -0.001014936933, 0.002439005479, 0.001767370829,
        -0.001035488877);
    vec3 x = value * 2.0 - 1.0;
    vec3 previous = vec3(1.0);
    vec3 current = x;
    vec3 result = coefficients[0] * previous + coefficients[1] * current;
    for (int degree = 2; degree <= 12; ++degree) {
        vec3 next = 2.0 * x * current - previous;
        result += coefficients[degree] * next;
        previous = current;
        current = next;
    }
    return result;
}

vec3 tone_map_agx(vec3 color)
{
    const mat3 linear_rec709_to_rec2020 = mat3(
        vec3(0.6274, 0.0691, 0.0164),
        vec3(0.3293, 0.9195, 0.0880),
        vec3(0.0433, 0.0113, 0.8956));
    const mat3 linear_rec2020_to_rec709 = mat3(
        vec3(1.6605, -0.1246, -0.0182),
        vec3(-0.5876, 1.1329, -0.1006),
        vec3(-0.0728, -0.0083, 1.1187));
    const mat3 inset = mat3(
        vec3(0.8566271533, 0.1373189729, 0.1118982130),
        vec3(0.0951212405, 0.7612419906, 0.0767994186),
        vec3(0.0482516061, 0.1014390365, 0.8113023684));
    const mat3 outset = mat3(
        vec3(1.1271005818, -0.1413297635, -0.1413297635),
        vec3(-0.1106066431, 1.1578237022, -0.1106066431),
        vec3(-0.0164939387, -0.0164939387, 1.2519364066));
    const float minimum_ev = -12.47393;
    const float maximum_ev = 4.026069;

    color = inset * (linear_rec709_to_rec2020 * max(color, vec3(0.0)));
    color = log2(max(color, vec3(1.0e-10)));
    color = clamp(
        (color - minimum_ev) / (maximum_ev - minimum_ev), 0.0, 1.0);
    color = agx_base_curve(color);
    color = outset * color;
    // Blender's AgX Base LUT is encoded for a Rec.1886 2.4 display before
    // conversion to the selected sRGB display.
    color = pow(max(color, vec3(0.0)), vec3(2.4));
    return clamp(linear_rec2020_to_rec709 * color, 0.0, 1.0);
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
        color = tone_map_agx(color);
        color = (color - 0.5) * contrast + 0.5;
        float luma = luminance(color);
        color = mix(vec3(luma), color, saturation);
    }
    frag_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
