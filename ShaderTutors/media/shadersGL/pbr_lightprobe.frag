#version 150

#include "pbr_common.head"

uniform samplerCube sampler1;	// diffuse irradiance
uniform samplerCube sampler2;	// specular irradiance
uniform sampler2D sampler3;		// preintegrated BRDF

in vec2 tex;
in vec3 wnorm;
in vec3 vdir;

out vec4 my_FragColor0;

void main()
{
	vec3 n = normalize(wnorm);
	vec3 v = normalize(vdir);
	vec3 r = 2 * dot(v, n) * n - v;

	float ndotv = clamp(dot(n, v), 0.0, 1.0);
	float miplevel = matParams.x * (NUM_MIPS - 1);

	vec4 fd					= BRDF_Lambertian(tex);
	vec3 diffuse_rad		= texture(sampler1, n).rgb * fd.rgb;
	vec3 specular_rad		= textureLod(sampler2, r, miplevel).rgb;
	vec2 f0_scale_bias		= texture(sampler3, vec2(ndotv, matParams.x)).rg;

	vec3 F0		= mix(vec3(0.04), baseColor.rgb, matParams.y);
	vec3 F		= F0 * f0_scale_bias.x + vec3(f0_scale_bias.y);

	my_FragColor0.rgb = diffuse_rad * fd.a + specular_rad * F;
	my_FragColor0.a = fd.a;
}
