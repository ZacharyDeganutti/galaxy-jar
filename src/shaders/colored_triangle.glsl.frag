#version 450

//shader input
layout (location = 3) in vec3 normal_interp;
layout (location = 4) in vec2 tex_interp;

//output write
layout (location = 0) out vec4 frag_color;

layout(set = 2, binding = 0) uniform sampler2D diffuse_tex;

void main() 
{
	vec4 diffuse = texture(diffuse_tex, tex_interp);
	if (diffuse.a < 0.1) {
		discard;
	}
	frag_color = diffuse;
}
