#version 450
// For descriptor sampling
#extension GL_EXT_nonuniform_qualifier : require 

//shader input
layout (location = 3) in vec3 normal_interp;

//output write
layout (location = 0) out vec4 frag_color;


void main() 
{
	vec3 inward = vec3(0.0f, 0.0f, 1.0f);
	float magnitude = dot(normal_interp, inward);
	// If it's an outer surface, set 0
	if (magnitude < 0.0f) {
		frag_color = vec4(0.0f);
	}
	// If it's an inner surface, color in and feather the edges
	else {
		// float adjusted = (magnitude > 0.5) ? 1.0f : 1.0f - pow(2.0f, -10.0f * magnitude);
		float adjusted = 1.0f - pow(2.0f, -30.0f * magnitude);
		frag_color = vec4(adjusted);
	}
}
