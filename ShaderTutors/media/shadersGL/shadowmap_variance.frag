#version 150

uniform vec2 clipPlanes;

in vec4 ltov;
out vec4 my_FragColor0;

void main()
{
	// linearized depth (handles ortho projection too)
	float d01 = (ltov.z * 0.5 + 0.5);
	float z = ((ltov.w < 0.0) ? -ltov.w : d01);
	float d = (z - clipPlanes.x) / (clipPlanes.y - clipPlanes.x);

	my_FragColor0 = vec4(d, d * d, 0.0, 0.0);
}
