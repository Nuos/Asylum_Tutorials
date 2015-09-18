#version 430

uniform sampler2D sampler0;
uniform sampler2D sampler1;		// shadowmap

uniform vec4 lightColor;
uniform vec2 clipPlanes;

in vec4 ltov;
in vec3 wnorm;
in vec3 ldir;
in vec3 vdir;
in vec2 tex;

out vec4 my_FragColor0;

float VarianceShadow(vec4 cpos)
{
	vec2 projpos = (cpos.xy / cpos.w) * 0.5 + 0.5;
	vec2 moments = texture(sampler1, projpos).xy;

	float d01		= (cpos.z * 0.5 + 0.5);
	float z			= ((cpos.w < 0.0) ? -cpos.w : d01);
	float d			= (z - clipPlanes.x) / (clipPlanes.y - clipPlanes.x);
	float mean		= moments.x;
	float variance	= max(moments.y - moments.x * moments.x, 1e-5);
	float md		= mean - d;
	float pmax		= variance / (variance + md * md);

	pmax = smoothstep(0.1, 1.0, pmax);

	return max(d <= mean ? 1.0 : 0.0, pmax);
}

//layout(early_fragment_tests) in;
void main()
{
	vec3 n = normalize(wnorm);
	vec3 l = normalize(ldir);
	vec3 v = normalize(vdir);
	vec3 h = normalize(v + l);

	float diff = clamp(dot(l, n), 0.0, 1.0);
	float spec = pow(clamp(dot(h, n), 0.0, 1.0), 80.0);

	vec4 base = texture(sampler0, tex);
	float s = VarianceShadow(ltov);

	my_FragColor0.rgb = s * lightColor.rgb * (base.rgb * diff + vec3(spec));
	my_FragColor0.a = 1.0;
}
