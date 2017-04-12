#version 150

#define PI				3.1415926535897932
#define PI_DOUBLE		6.2831853071795864
#define PI_HALF			1.5707963267948966
#define ONE_OVER_PI		0.3183098861837906

#ifndef NUM_STEPS
#define NUM_STEPS		8
#endif

#define RADIUS			2.0		// in world space

uniform sampler2D gbufferDepth;
uniform sampler2D gbufferNormals;
uniform sampler2D noise;

uniform vec4 projInfo;
uniform vec4 clipInfo;
uniform vec3 eyePos;
uniform vec2 invRes;
uniform vec4 params;

in vec2 tex;

#ifdef NUM_DIRECTIONS
out vec4 my_FragColor0;
#else
out float my_FragColor0;
#endif

vec4 GetViewPosition(vec2 uv)
{
	float d = texture(gbufferDepth, uv * invRes).r;
	vec4 ret = vec4(0.0, 0.0, 0.0, d);

	ret.z = clipInfo.x + d * (clipInfo.y - clipInfo.x);
	ret.xy = (uv * projInfo.xy + projInfo.zw) * ret.z;

	return ret;
}

float Falloff(float dist2, float cosh)
{
#define FALLOFF_START2	0.16
#define FALLOFF_END2	4.0

#ifdef NUM_DIRECTIONS
	return 0.0;	// disable falloff for reference
#else
	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
#endif
}

void main()
{
	ivec2 loc = ivec2(gl_FragCoord.xy);
	vec4 vpos = GetViewPosition(gl_FragCoord.xy);

	if( vpos.w == 1.0 ) {
#ifdef NUM_DIRECTIONS
		my_FragColor0 = vec4(0.0, 0.0103, 0.0707, 1.0);
#else
		my_FragColor0 = 1.0;
#endif
		return;
	}

	vec4 s;
	vec3 vnorm	= texelFetch(gbufferNormals, loc, 0).rgb;
	vec3 vdir	= normalize(-vpos.xyz);
	vec3 dir, ws;

	// calculation uses left handed system
	vnorm.z = -vnorm.z;

	vec2 noises	= texelFetch(noise, loc % 4, 0).rg;
	vec2 offset;
	vec2 horizons = vec2(-1.0, -1.0);

	float radius = (RADIUS * clipInfo.z) / vpos.z;

	radius = max(NUM_STEPS, radius);
	//radius = min(1.0 / (2.0 * invRes.y), radius);

	float stepsize	= radius / NUM_STEPS;
	float phi		= (params.x + noises.x) * PI;
	float ao		= 0.0;
	float currstep	= mod(params.y + noises.y, 1.0) * (stepsize - 1.0) + 1.0;
	float dist2, invdist, falloff, cosh;

#ifdef NUM_DIRECTIONS
	for( int k = 0; k < NUM_DIRECTIONS; ++k ) {
		phi = float(k) * (PI / NUM_DIRECTIONS);
		currstep = 1.0;
#endif

		dir = vec3(cos(phi), sin(phi), 0.0);
		horizons = vec2(-1.0);

		// calculate horizon angles
		for( int j = 0; j < NUM_STEPS; ++j ) {
			offset = round(dir.xy * currstep);
			currstep += stepsize;

			s = GetViewPosition(gl_FragCoord.xy + offset);
			ws = s.xyz - vpos.xyz;

			dist2 = dot(ws, ws);
			invdist = inversesqrt(dist2);
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist2, cosh);
			horizons.x = max(horizons.x, cosh - falloff);

			s = GetViewPosition(gl_FragCoord.xy - offset);
			ws = s.xyz - vpos.xyz;

			dist2 = dot(ws, ws);
			invdist = inversesqrt(dist2);
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist2, cosh);
			horizons.y = max(horizons.y, cosh - falloff);
		}

		horizons = acos(horizons);

		// calculate gamma
		vec3 bitangent	= normalize(cross(dir, vdir));
		vec3 tangent	= cross(vdir, bitangent);
		vec3 nx			= vnorm - bitangent * dot(vnorm, bitangent);

		float nnx		= length(nx);
		float invnnx	= 1.0 / (nnx + 1e-6);			// to avoid division with zero
		float cosxi		= dot(nx, tangent) * invnnx;	// xi = gamma + PI_HALF
		float gamma		= acos(cosxi) - PI_HALF;
		float cosgamma	= dot(nx, vdir) * invnnx;
		float singamma2	= -2.0 * cosxi;					// cos(x + PI_HALF) = -sin(x)

		// clamp to normal hemisphere
		horizons.x = gamma + max(-horizons.x - gamma, -PI_HALF);
		horizons.y = gamma + min(horizons.y - gamma, PI_HALF);

		// Riemann integral is additive
		ao += nnx * 0.25 * (
			(horizons.x * singamma2 + cosgamma - cos(2.0 * horizons.x - gamma)) +
			(horizons.y * singamma2 + cosgamma - cos(2.0 * horizons.y - gamma)));

#ifdef NUM_DIRECTIONS
	}

	// PDF = 1 / pi and must normalize with pi as of Lambert
	ao = ao / float(NUM_DIRECTIONS);
	my_FragColor0 = vec4(ao, ao, ao, 1.0);
#else
	my_FragColor0 = ao;
#endif
}
