#version 150

in vec3 my_Position;
in vec3 my_Normal;
in vec2 my_Texcoord0;
in vec3 my_Tangent;
in vec3 my_Binormal;

uniform mat4 matWorldView;
uniform mat4 matWorldViewInv;
uniform mat4 matProj;

out vec3 vnorm;
out vec3 vtan;
out vec3 vbin;

out vec4 vpos;
out vec2 tex;

void main()
{
	vpos = matWorldView * vec4(my_Position, 1.0);
	tex = my_Texcoord0;

	vnorm = (vec4(my_Normal, 0.0) * matWorldViewInv).xyz;
	vtan = (matWorldView * vec4(my_Tangent, 0.0)).xyz;
	vbin = (matWorldView * vec4(my_Binormal, 0.0)).xyz;

	gl_Position = matProj * vpos;
}
