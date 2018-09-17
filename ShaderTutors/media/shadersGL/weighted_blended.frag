#version 430

uniform sampler2D sampler0;
uniform vec4 matAmbient;

in vec2 tex;

out vec4 my_FragColor0;
out vec4 my_FragColor1;

void main()
{
	vec4 base = texture(sampler0, tex);
	base *= matAmbient;

	float tmp = (1.0 - 0.99 * gl_FragCoord.z) * base.a * 10.0;
	float weight = clamp(tmp, 0.01, 30.0);

	my_FragColor0 = vec4(base.rgb * base.a, base.a) * weight;
	my_FragColor1 = vec4(base.a);
}
