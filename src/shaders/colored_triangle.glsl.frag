#version 450

//shader input
layout (location = 3) in vec3 normal_interp;

//output write
layout (location = 0) out vec4 frag_color;

void main() 
{
	//return normal
	frag_color = vec4((normalize(normal_interp) * 0.5f ) + 0.5f, 1.0);
}
