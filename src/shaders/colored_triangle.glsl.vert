#version 450

//shader input
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coord;

layout (location = 3) out vec3 normal_interp;

//descriptor bindings for the pipeline
layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4x4 modelview;
} ubo;

float inv_aspect = 600.0/800.0;
float fov = 45.0;
float tan_half_fov = tan(fov/2.0);
float near = 100;
float far = 2000.0;

float turn = radians(90);

mat4x4 cam_rotation = ubo.modelview * transpose(mat4x4(
	cos(turn), 0, sin(turn), 0,
	0, 1, 0, 0,
	-sin(turn), 0, -cos(turn), 0,
	0, 0, 0, 1
));

mat4x4 camera = cam_rotation * transpose(mat4x4(
						1, 0, 0, 0,
						0, -1, 0, 100,
						0, 0, 1, 0,
						0, 0, 0, 1));

mat4x4 proj_ex = transpose(mat4x4(inv_aspect/tan_half_fov, 0, 0, 0,
						0, 1.0/tan_half_fov, 0, 0,
						0, 0, far/(far-near), -(near*far)/(far-near),
						0, 0, 1, 0));

void main() 
{
	//output the position of each vertex
	gl_Position = proj_ex * camera * vec4(vertex, 1.0f);
	normal_interp = normal;
}
