#version 150

// NOTE: normal mapping amplifies the "waving effect" due to disocclusions
//#define USE_NORMAL_MAPPING

uniform sampler2D sampler0;
uniform sampler2D sampler1;

uniform vec4 clipPlanes;

in vec3 vnorm;
in vec3 vtan;
in vec3 vbin;

in vec4 vpos;
in vec2 tex;

out vec4 my_FragColor0;
out vec4 my_FragColor1;
out float my_FragColor2;

void main()
{
#ifdef USE_NORMAL_MAPPING
	vec3 tnorm = texture(sampler1, tex).xyz;
	tnorm = tnorm * 2.0 - vec3(1.0);

	vec3 t = normalize(vtan);
	vec3 b = normalize(vbin);
#endif

	vec3 n = normalize(vnorm);

	// view space is right-handed
	float d = (-vpos.z - clipPlanes.x) / (clipPlanes.y - clipPlanes.x);

#ifdef USE_NORMAL_MAPPING
	mat3 tbn = mat3(t, b, n);
	n = tbn * tnorm;
#endif

	my_FragColor0 = texture(sampler0, tex);
	my_FragColor1 = vec4(n, 1.0);
	my_FragColor2 = d;
}
