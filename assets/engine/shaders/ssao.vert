#version 330 core

out vec2 uv;

void main() 
{
    float x = float(((uint(gl_VertexID) + 2u) / 3u)%2u); 
    float y = float(((uint(gl_VertexID) + 1u) / 3u)%2u); 

    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
    uv = vec2(x, y);
}

/*#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 i_vertex_pos;
layout(location = 1) in vec2 i_vertex_uv;
layout(location = 2) in vec3 i_normal_pos;
layout(location = 3) in vec3 i_tangent_pos;

// Output data will be interpolated for each fragment.
out vec3 normal_pos_view;
out vec3 tangent_pos_view;
out vec3 bitangent_pos_view;
out vec3 vertex_pos_view;
out vec2 uv;

// Values that stay constant for the whole mesh.
uniform mat4 model; // model matrix.
uniform mat4 view; // view matrix.
uniform mat4 projection; // projection matrix.

void main(){

	// Output position of the vertex, in clip space : MVP * position.
    gl_Position = projection* view * model * vec4(i_vertex_pos, 1);

    vertex_pos_view = vec3(view * model * vec4(i_vertex_pos, 1.0));
	
	// Convert normal basis vectors into world space.
	normal_pos_view = vec3(view * model * vec4(i_normal_pos, 1.0));
	tangent_pos_view = vec3(view * model * vec4(i_tangent_pos, 1.0));
	bitangent_pos_view = cross(tangent_pos_view, normal_pos_view);

    // UV of the vertex.
    uv = i_vertex_uv;

}*/