#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D source_texture;

void main()
{
	frag_color = texture(source_texture, uv);
}
