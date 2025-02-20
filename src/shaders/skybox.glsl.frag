#version 450

// For descriptor sampling
#extension GL_EXT_nonuniform_qualifier : require 

//shader input
layout (location = 3) in vec3 sampling_direction;

//output write
layout (location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform samplerCube combined_img_samplers[];

layout( push_constant ) uniform PushConstants
{
	uint skybox_texture_index;
} indices;

void main() 
{
	vec4 sky = texture(combined_img_samplers[nonuniformEXT(indices.skybox_texture_index)], normalize(sampling_direction));
	frag_color = sky;
}
