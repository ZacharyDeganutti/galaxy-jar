#version 450

//shader input
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coord;
layout (location = 3) out vec3 normal_interp;
layout (location = 4) out vec2 tex_interp;
layout (location = 5) out vec3 position_interp;

//descriptor bindings for the pipeline
layout(set = 0, binding = 0) uniform ViewMatrix {
	mat4x4 data;
} view;

layout(set = 1, binding = 0) uniform ProjectionMatrix {
	mat4x4 data;
} projection;

layout(set = 6, binding = 0) uniform ModelMatrix {
	mat4x4 data;
} model;

void main() 
{
	gl_Position = projection.data * view.data * model.data * vec4(vertex, 1.0f);
	normal_interp = normalize(transpose(inverse(mat3(view.data) * mat3(model.data))) * normal);
	tex_interp = tex_coord;
	position_interp = (view.data * model.data * vec4(vertex, 1.0f)).xyz;
}
