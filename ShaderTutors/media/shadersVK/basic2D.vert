
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 my_Position;
layout (location = 1) in vec2 my_Texcoord0;

layout (push_constant) uniform TransformData {
	mat4 matTexture;
} transform;

layout (location = 0) out vec2 tex;

out gl_PerVertex {
	vec4 gl_Position;
};

void main()
{
	tex = (transform.matTexture * vec4(my_Texcoord0, 0, 1)).xy;
	gl_Position = vec4(my_Position, 1);
}
