#version 450
// For descriptor sampling
#extension GL_EXT_nonuniform_qualifier : require 

//shader input
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coord;
layout (location = 3) out vec3 normal_interp;

//descriptor bindings for the pipeline
layout(set = 0, binding = 0) uniform Transforms {
	mat4 view;
    mat4 projection;
    vec4 sun_direction;
} transforms;

layout(set = 2, binding = 0) uniform ModelMatrix {
	mat4x4 data;
} model;

void main() 
{
	gl_Position = transforms.projection * transforms.view * model.data * vec4(vertex, 1.0f);
	normal_interp = normalize(transpose(inverse(mat3(transforms.view) * mat3(model.data))) * normal);
}
