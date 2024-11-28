#version 450

//shader input
layout (location = 3) in vec3 normal_interp;

//output write
layout (location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform samplerCube skybox;

void main() 
{
	//return normal
	// frag_color = ubo.brightness * vec4((normalize(normal_interp) * 0.5f ) + 0.5f, 1.0);
	vec4 sky = texture(skybox, normalize(normal_interp));
	frag_color = sky;
}
