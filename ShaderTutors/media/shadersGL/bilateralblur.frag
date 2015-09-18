#version 150

#define EDGE_SHARPNESS	1.0
#define SCALE			2

uniform sampler2D sampler0;
uniform vec3 axis;	// z = what to write to fragcolor

in vec2 tex;
out vec4 my_FragColor0;

float UnpackKey(vec2 p)
{
	return p.x * (256.0 / 257.0) + p.y * (1.0 / 257.0);
}

vec2 GaussTap(ivec2 loc, ivec2 dir, float key, float weight)
{
	ivec2	taploc	= loc + dir * SCALE;
	vec4	tap		= texelFetch(sampler0, taploc, 0);
	float	tapkey	= UnpackKey(tap.gb);
	float	w		= 0.3 + weight;

	w *= max(0.0, 1.0 - (EDGE_SHARPNESS * 2000.0) * abs(tapkey - key));

	//return vec2(tap.x * w, w);
	return vec2(tap.x * weight, weight);
}

void main()
{
	float gaussian[5];

	gaussian[0] = 0.153170;
	gaussian[1] = 0.144893;
	gaussian[2] = 0.122649;
	gaussian[3] = 0.092902;
	gaussian[4] = 0.062970;

	ivec2	loc			= ivec2(gl_FragCoord.xy);
	ivec2	iaxis		= ivec2(axis.xy);
	vec4	temp		= texelFetch(sampler0, loc, 0);
	vec2	sum_weight	= vec2(temp.x * gaussian[0], gaussian[0]);
	float	key			= UnpackKey(temp.gb);

	if( key == 1.0 )
	{
		my_FragColor0 = temp;
		return;
	}

	sum_weight += GaussTap(loc, -4 * iaxis, key, gaussian[4]);
	sum_weight += GaussTap(loc, -3 * iaxis, key, gaussian[3]);
	sum_weight += GaussTap(loc, -2 * iaxis, key, gaussian[2]);
	sum_weight += GaussTap(loc, -1 * iaxis, key, gaussian[1]);

	sum_weight += GaussTap(loc, 1 * iaxis, key, gaussian[1]);
	sum_weight += GaussTap(loc, 2 * iaxis, key, gaussian[2]);
	sum_weight += GaussTap(loc, 3 * iaxis, key, gaussian[3]);
	sum_weight += GaussTap(loc, 4 * iaxis, key, gaussian[4]);

	sum_weight.x /= (sum_weight.y + 0.0001);

	vec4 color1 = vec4(sum_weight.x, temp.gb, 1.0);
	vec4 color2 = vec4(sum_weight.xxx, 1.0);

	my_FragColor0 = mix(color1, color2, axis.z);
}
