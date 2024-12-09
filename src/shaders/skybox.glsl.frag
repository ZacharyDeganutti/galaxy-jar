#version 450

//shader input
layout (location = 3) in vec3 sampling_direction;

//output write
layout (location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform samplerCube skybox;

void main() 
{
	vec4 sky = texture(skybox, normalize(sampling_direction));
	frag_color = sky;
}
