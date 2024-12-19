#version 450

//shader input
layout (location = 3) in vec3 normal_interp;
layout (location = 4) in vec2 tex_interp;
layout (location = 5) in vec3 position_interp;

//output write
layout (location = 0) out vec4 frag_color;


layout(set = 3, binding = 0) uniform sampler2D diffuse_tex;
layout(set = 4, binding = 0) uniform sampler2D normal_tex;
layout(set = 5, binding = 0) uniform sampler2D specular_tex;
layout(set = 2, binding = 0) uniform SunDirection {
	vec4 data;
} sun_direction;

void main() 
{
	vec4 albedo = texture(diffuse_tex, tex_interp);
	if (albedo.a < 0.1) {
		discard;
	}
	vec4 ambient = vec4(0.1f, 0.1f, 0.1f, 0.0f);
	frag_color = texture(specular_tex, tex_interp);
	frag_color = texture(normal_tex, tex_interp);
	frag_color = albedo * (ambient + max(dot(normalize(sun_direction.data), vec4(normalize(normal_interp), 0.0f)), 0.0f)) ;
	frag_color = frag_color / (vec4(vec3(1.0), 0.0) + frag_color);
}
