#version 450

//shader input
layout (location = 3) in vec3 normal_interp;
layout (location = 4) in vec2 tex_interp;
layout (location = 5) in vec3 position_interp;

//output write
layout (location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform ViewMatrix {
	mat4x4 data;
} view;

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

float angle_term(vec3 light_direction, vec3 normal_direction) {
	return max(dot(normalize(light_direction), (normal_direction)), 0.0f);
}

vec3 unpack_normal(vec2 packed_normal) {
	vec2 expanded = packed_normal * 2.0f - 1.0f;
	float z = sqrt(1.0f - expanded.x * expanded.x - expanded.y * expanded.y);
	return vec3 (expanded, z);
}

float f_fresnel_schlick_simplified_dielectric(vec3 surface_normal, vec3 incident_vector) {
	float constant_term_squared = 0.04; // ((n1 - n2) / (n1 + n2))^2 where n1 and n2 are indices of refraction of air and a generic material (1.0 and 1.5)
	return constant_term_squared + (1 - constant_term_squared) * pow((1 - max(dot(incident_vector, surface_normal), 0.0f)), 5);
}

// Returns 1.0f for positive values, 0.0f for negative
float positive_characteristic(float value) {
	return (1.0f + sign(value))/2.0f;
}

float calculate_alpha_squared(float roughness) {
	return pow(roughness, 4.0f); // alpha = roughness^2
}

float d_ndf_ggx(float alpha_squared, vec3 normal, vec3 halfway) {
	float normal_dot_halfway = max(dot(normal, halfway), 0.0f);
	float numerator = positive_characteristic(normal_dot_halfway) * alpha_squared; // 1 if dot product positive, 0 if negative
	float denominator = 4 * pow(1.0f + (normal_dot_halfway * normal_dot_halfway) * (alpha_squared - 1.0f), 2.0f);
	return numerator / denominator;
}

float g_smith_g1_ggx(float alpha_squared, vec3 halfway, vec3 view_dir) {
	float numerator = positive_characteristic(max(dot(halfway, view_dir), 0.0f));
	float lambda_ggx = 0.5f * (-1.0f + sqrt(1.0f + (1.0f/alpha_squared)));
	float denominator = 1.0f + lambda_ggx;
	return numerator / denominator;
}

// light and view vectors originate from the surface outward. All input vectors assumed prenormalized.
float compute_specular_reflectance(float roughness, float fresnel_value, vec3 normal, vec3 light, vec3 view_dir) {
	vec3 halfway = normalize(view_dir + light);
	float alpha_squared = calculate_alpha_squared(roughness);
	float numerator = d_ndf_ggx(alpha_squared, normal, halfway) * g_smith_g1_ggx(alpha_squared, halfway, view_dir) * fresnel_value;
	float denominator = 4.0f * max(dot(normal, light), 0.0f) * max(dot(normal, view_dir), 0.0f) + 0.0001f;
	return numerator / denominator;
}

// normal, light, and view are in tangent space
vec4 total_reflectance(vec4 albedo, vec4 ambient_color, vec4 incident_color, float roughness, float metalness, vec3 normal, vec3 light, vec3 view_dir) {
	vec4 diffuse_color = lambertian_term(albedo);
	float dielectric_fresnel_value = f_fresnel_schlick_simplified_dielectric(normal, view_dir);
	float conductive_fresnel_value = f_fresnel_schlick_simplified_dielectric(normal, view_dir); // TODO: Figure out sensible values for this function
	vec4 dielectric_reflective_color = incident_color * compute_specular_reflectance(roughness, dielectric_fresnel_value, normal, light, view_dir);
	vec4 conductive_reflective_color = incident_color * compute_specular_reflectance(roughness, conductive_fresnel_value, normal, light, view_dir);
	float angle_factor = angle_term(light, normal);
	
	vec4 dielectric_portion = mix(diffuse_color, dielectric_reflective_color, 1-dielectric_fresnel_value);
	//vec4 dielectric_portion = diffuse_color;
	vec4 conductive_portion = conductive_reflective_color;
	// return mix(conductive_portion, dielectric_portion, metalness) * angle_factor;
	// Cheat with omnidirectional ambient light until IBL is possible
	return ambient_color * diffuse_color + dielectric_portion * angle_factor;
	// return (diffuseness * diffuse_color + metalness * reflective_color) * angle_factor; 
}

void main() 
{
	vec4 albedo = texture(diffuse_tex, tex_interp);
	vec4 ambient = vec4(0.2f, 0.2f, 0.2f, 0.0f);
	vec4 sun_direction_transformed = vec4(normalize((view.data * sun_direction.data).xyz), 0.0);
	frag_color = texture(normal_tex, tex_interp);
	
	// Sample normal from map, unpack RG components
	vec2 packed_normal = texture(normal_tex, tex_interp).xy;
	vec3 tangent_space_normal = unpack_normal(packed_normal);

	// Get TBN basis from interpolated normal, view space position, and uv. Transform light direction to tangent space
	mat4 tbn_basis = compute_tbn(position_interp, normalize(normal_interp), tex_interp);
	vec3 tangent_space_light = normalize((tbn_basis * sun_direction_transformed).xyz);

	if (albedo.a < 0.1) {
		discard;
	}

	// frag_color = albedo * (ambient + lambertian_diffuse(tangent_space_light, tangent_space_normal));
	// Update this to sample from cubemap, sky blue for now
	vec4 incident_color = vec4(0.53f, 0.81f, 0.92f, 1.0f);
	vec4 specular_map_sample = texture(specular_tex, tex_interp);
	float roughness = specular_map_sample.r;
	float metalness = specular_map_sample.g;
	vec3 tangent_space_view = normalize((tbn_basis * vec4(normalize(-position_interp), 0.0f)).xyz);
	frag_color = total_reflectance(albedo, ambient, incident_color, roughness, metalness, tangent_space_normal, tangent_space_light.xyz, tangent_space_view);
	//frag_color = total_reflectance(albedo, ambient, incident_color, roughness, metalness, normalize(normal_interp.xyz), sun_direction_transformed.xyz, normalize(-position_interp.xyz));
	frag_color = frag_color / (vec4(vec3(1.0f), 0.0f) + frag_color);

}
