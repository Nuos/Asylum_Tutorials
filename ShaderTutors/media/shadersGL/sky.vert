
#version 150

in vec3 my_Position;

uniform vec3 eyePos;
uniform mat4 matViewProj;
uniform mat4 matWorld;

out vec3 view;

void main()
{
	vec4 wpos = matWorld * vec4(my_Position, 1.0);
	view = wpos.xyz - eyePos;

	gl_Position = matViewProj * wpos;
}
