#version 150

#define AO_RADIUS			0.25	// world space
#define NUM_SAMPLES			15
#define NUM_SPIRAL_TURNS	7

uniform sampler2D sampler0;

uniform vec4 clipInfo;
uniform vec4 projInfo;

in vec2 tex;
out vec4 my_FragColor0;

void PackKey(float key, out vec2 p)
{
	float temp = floor(key * 256.0);

	p.x = temp * (1.0 / 256.0);
	p.y = key * 256.0 - temp;
}

vec4 GetViewPosition(ivec2 spos)
{
	float d = texelFetch(sampler0, spos, 0).r;
	float z = clipInfo.x / (clipInfo.y * d + clipInfo.z);
	vec2 center = vec2(spos) + vec2(0.5);

	return vec4((center * projInfo.xy + projInfo.zw) * z, z, d);
}

vec3 SampleDisk(int i, float spinangle)
{
	float alpha = (float(i) + 0.5) / NUM_SAMPLES;
	float angle = alpha * (NUM_SPIRAL_TURNS * 6.28) + spinangle;

	return vec3(cos(angle), sin(angle), alpha);
}

float SampleAO(ivec2 pixel, vec3 p, vec3 n, float diskradius, int i, float angle)
{
	vec3 sdir = SampleDisk(i, angle);
	ivec2 offpix = pixel + ivec2(sdir.xy * sdir.z * diskradius);

	vec4 Q = GetViewPosition(offpix);

	if( Q.w > 0.9998 )
		return 0.0;

	vec3 v = Q.xyz - p;

	float vv = dot(v, v);
	float vn = dot(v, n);
	float f = max(AO_RADIUS * AO_RADIUS - vv, 0.0);

	const float epsilon = 0.01;
	const float bias = 0.01;

	return f * f * f * max((vn - bias) / (epsilon + vv), 0.0);
}

void main()
{
	ivec2 loc = ivec2(gl_FragCoord.xy);

	vec4 pos = GetViewPosition(loc);

	if( pos.w > 0.9998 )
	{
		my_FragColor0 = vec4(1.0);
		return;
	}

	vec3 norm = normalize(cross(dFdy(pos.xyz), dFdx(pos.xyz)));

	float randomangle = float((3 * loc.x ^ loc.y + loc.x * loc.y) * 10);
	float diskradius = (clipInfo.w * AO_RADIUS) / pos.z;
	float sum = 0.0;

	for (int i = 0; i < NUM_SAMPLES; ++i) {
		sum += SampleAO(loc, pos.xyz, norm, diskradius, i, randomangle);
	}

	float sigma = 5.0 / (NUM_SAMPLES * pow(AO_RADIUS, 6.0));
	float A = max(0.0, 1.0 - sum * sigma);

	PackKey(clamp(pos.z / clipInfo.z, 0.0, 1.0), my_FragColor0.gb);

	if (abs(dFdx(pos.z)) < 0.02)
		A -= dFdx(A) * ((loc.x & 1) - 0.5);

	if (abs(dFdy(pos.z)) < 0.02)
		A -= dFdy(A) * ((loc.y & 1) - 0.5);

	my_FragColor0.r = A;
	my_FragColor0.a = 1.0;
}
