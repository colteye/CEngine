#version 330 core

in vec2 uv;
in vec4 color;

uniform sampler2D particle_texture;
uniform bool use_texture;
uniform int blend_mode;

out vec4 output_color;

void main()
{
    vec4 sampled = texture(particle_texture, uv);
    if (!use_texture)
    {
        float distance_from_center = length(uv - vec2(0.5));
        sampled = vec4(1.0, 1.0, 1.0, 1.0 - smoothstep(0.18, 0.5, distance_from_center));
    }

    output_color = color * sampled;
    if (blend_mode == 2)
        output_color.rgb *= output_color.a;
}
