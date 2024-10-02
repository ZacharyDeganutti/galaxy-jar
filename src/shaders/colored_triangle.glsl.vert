#version 450

//shader input
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coord;

layout (location = 0) out vec3 normal_out;

void main() 
{
	//output the position of each vertex
	gl_Position = vec4(vertex, 1.0f);
	normal_out = normal;
}
