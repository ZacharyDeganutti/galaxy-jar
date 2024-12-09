#version 450

//shader input
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coord;

//output
layout (location = 3) out vec3 sampling_direction;

//descriptor bindings for the pipeline
layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4x4 cam_rotation_in;
} ubo;

float inv_aspect = 600.0/800.0;
float fov = 45.0;
float tan_half_fov = tan(fov/2.0);
float near = 0.1;
float far = 10.0;

mat4x4 flip_y = transpose(mat4x4(
						1, 0, 0, 0,
						0, -1, 0, 0,
						0, 0, 1, 0,
						0, 0, 0, 1));

mat4x4 proj_ex = transpose(mat4x4(inv_aspect/tan_half_fov, 0, 0, 0,
						0, 1.0/tan_half_fov, 0, 0,
						0, 0, far/(far-near), -(near*far)/(far-near),
						0, 0, 1, 0));

void main() 
{
	// Project the vertex and flip it across the y to match vulkan clip space
	vec4 upright_projection = proj_ex * vec4(vertex, 1.0f);
	gl_Position = flip_y * upright_projection;
	// Piggyback on the frustum shape created by projection to generate a bunch of vectors to sample the skybox
	// Use the upright projection because cubemap sampling is setup to use Y-up
	// Rotate the ray directions around relative to the inverse of the camera rotation
	sampling_direction = normalize(transpose(ubo.cam_rotation_in) * upright_projection).xyz;
}
