#version 150

in vec3 my_Position;

out vec4 cpos;

void main()
{
	cpos = vec4(my_Position, 1.0);
	gl_Position = cpos;
}
