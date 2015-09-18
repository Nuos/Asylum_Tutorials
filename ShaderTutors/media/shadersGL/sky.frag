
#version 150

uniform float gamma;
uniform samplerCube sampler0;

in vec3 view;

out vec4 my_FragColor0;

void main()
{
	vec4 color = texture(sampler0, view);
	color.rgb = pow(color.rgb, vec3(gamma));

	my_FragColor0 = color;
}
