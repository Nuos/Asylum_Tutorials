#version 150

#include "pbr_common.head"

uniform sampler2D sampler1;		// normal map TODO
uniform vec3 lightColor;

uniform float lumIntensity;
uniform float invRadius;

in vec2 tex;
in vec3 wnorm;
in vec3 vdir;
in vec3 ldir;

out vec4 my_FragColor0;

void main()
{
	vec3 n = normalize(wnorm);
	vec3 v = normalize(vdir);
	vec3 l = normalize(ldir);
	vec3 h = normalize(v + l);

	float ndotv = clamp(dot(n, v), 0.0, 1.0);
	float ndotl = clamp(dot(n, l), 0.0, 1.0);
	float ndoth = clamp(dot(n, h), 0.0, 0.999); // NOTE: 1 causes inf sometimes
	float ldoth = clamp(dot(l, h), 0.0, 1.0);

	vec4 fd = BRDF_Lambertian(tex);
	vec3 fs = BRDF_CookTorrance(ldoth, ndoth, ndotv, ndotl, matParams.x);

	float dist		= length(ldir);
	float dist2		= max(dot(ldir, ldir), 1e-4);
	float falloff	= (lumIntensity / dist2) * max(0.0, 1.0 - dist * invRadius);

	float fade		= max(0.0, (fd.a - 0.75) * 4.0);
	float shadow	= mix(1.0, falloff, fade);

	vec3 final_color = (fd.rgb * fd.a + fs) * ndotl * shadow; // * lightColor

	my_FragColor0.rgb = final_color;
	my_FragColor0.a = fd.a;
}
