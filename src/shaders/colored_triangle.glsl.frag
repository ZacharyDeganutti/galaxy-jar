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

mat4 compute_tbn(in vec3 p, in vec3 n, in vec2 uv)
{
    // get the necessary derivatives
    vec3 dpdx = dFdx( p );
    vec3 dpdy = dFdy( p );
    vec2 duvdx = dFdx( uv );
    vec2 duvdy = dFdy( uv );

	// Jacobian is [[dudx dudy] [dvdx dvdy]], get inv determinant
	float inv_jacobian_det = 1.0f / (duvdx.x * duvdy.y - duvdy.x * duvdx.y);
	float dxdu = duvdy.y * inv_jacobian_det;
	float dydu = -1.0f * duvdy.x * inv_jacobian_det;

	// Project point derivatives onto tangent plane
	vec3 dpdx_s = dpdx - dot(dpdx, n) * n;
  	vec3 dpdy_s = dpdy - dot(dpdy, n) * n;

	// Plug it in to get T
	vec3 tangent = normalize(dxdu * dpdx_s + dydu * dpdy_s);
	vec3 bitangent = cross(tangent, n);

	mat4 result = mat4(1.0f);
	result[0] = vec4(tangent, 0.0f);
	result[1] = vec4(bitangent, 0.0f);
	result[2] = vec4(n, 0.0f);

	return transpose(result);
}

vec4 lambertian_term(vec4 albedo) {
	float pi = 3.1415926538;
	return albedo / pi;
}

float lambertian_diffuse(vec3 light_direction, vec3 normal_direction) {
	return max(dot(normalize(light_direction), (normal_direction)), 0.0f);
}

vec3 unpack_normal(vec2 packed_normal) {
	vec2 expanded = packed_normal * 2.0f - 1.0f;
	float z = sqrt(1.0f - pow(expanded.x, 2.0f) - pow(expanded.y, 2.0f));
	return vec3 (expanded, z);
}


void main() 
{
	vec4 albedo = texture(diffuse_tex, tex_interp);
	vec4 ambient = vec4(0.2f, 0.2f, 0.2f, 0.0f);
	frag_color = texture(specular_tex, tex_interp);
	frag_color = texture(normal_tex, tex_interp);
	
	// Sample normal from map, unpack RG components
	vec2 packed_normal = texture(normal_tex, tex_interp).xy;
	vec3 normal_3d = unpack_normal(packed_normal);

	// Get TBN basis from interpolated normal, world position, and uv. Transform light direction to tangent space
	mat4 tbn_basis = compute_tbn(position_interp, normalize(normal_interp), tex_interp);
	vec4 tangent_space_light = tbn_basis * sun_direction.data;

	if (albedo.a < 0.1) {
		discard;
	}
	// Bump map
	frag_color = albedo * (ambient + lambertian_diffuse(tangent_space_light.xyz, normal_3d));
	frag_color = frag_color / (vec4(vec3(1.0f), 0.0f) + frag_color);
}
