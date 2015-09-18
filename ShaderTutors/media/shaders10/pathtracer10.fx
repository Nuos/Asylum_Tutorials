
#define FLT_MAX			3.40282347E+38
#define PI				3.1415926535897932
#define ONE_OVER_PI		0.3183098861837906
#define TRACE_DEPTH		5
#define NUM_OBJECTS		17

uniform Texture2D	prevIteration;
uniform Texture2D	sceneTexture;

uniform matrix		matViewProjInv;
uniform float3		eyePos;
uniform float2		screenSize;
uniform float		time;
uniform float		currSample;

SamplerState pointSampler
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

SamplerState linearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

// =======================================================================
//
// Scene objects
//
// =======================================================================

struct SceneObject
{
	int		type;		// 0 -> light, 1 -> plane, 2 -> sphere
	float4	params;
	float3	baseColor;
	float	roughness;
	bool	metal;
};

static const SceneObject objects[NUM_OBJECTS] =
{
	{ 0, float4(-2.0f, 3.8f, 2.0f, 0.5f),	float3(10, 10, 10),				0, false },		// light
	{ 1, float4(0, 1, 0, 0),				float3(0.664f, 0.824f, 0.85f),	0.3f, true },	// bottom
	{ 1, float4(-1, 0, 0, 2.0f),			float3(1, 0, 0),				1.0f, false },	// right
	{ 1, float4(0, 0, -1, 2.35f),			float3(1, 1, 1),				0.9f, false },	// back
	{ 1, float4(1, 0, 0, 2.0f),				float3(0, 1, 0),				1.0f, false },	// left
	{ 1, float4(0, -1, 0, 4.0f),			float3(1, 1, 1),				0.9f, false },	// top
	{ 1, float4(0, 0, 1, 5.0f),				float3(1, 1, 1),				0.9f, false },	// front
	{ 2, float4(-1.3f, 0.5f, 0.5f, 0.5f),	float3(1.0f, 0.3f, 0.1f),		0.1f, false },	// sphere1
	{ 2, float4(1.2f, 0.5f, 0.25f, 0.5f),	float3(0.1f, 0.3f, 1.0f),		0.1f, false },	// sphere2
	{ 2, float4(0, 0.75f, 1.2f, 0.75f),		float3(1.022f, 0.782f, 0.344f),	0.2f, true },	// sphere3

	{ 2, float4(-1.6875f, 0.25f, -0.85f, 0.25f),	float3(0.972f, 0.96f, 0.915f),	0.0f, true },	// silver sphere 1
	{ 2, float4(-1.125f, 0.25f, -0.85f, 0.25f),		float3(0.972f, 0.96f, 0.915f),	0.16f, true },	// silver sphere 2
	{ 2, float4(-0.5625f, 0.25f, -0.85f, 0.25f),	float3(0.972f, 0.96f, 0.915f),	0.32f, true },	// silver sphere 3
	{ 2, float4(0, 0.25f, -0.85f, 0.25f),			float3(0.972f, 0.96f, 0.915f),	0.48f, true },	// silver sphere 4
	{ 2, float4(0.5625f, 0.25f, -0.85f, 0.25f),		float3(0.972f, 0.96f, 0.915f),	0.64f, true },	// silver sphere 5
	{ 2, float4(1.125f, 0.25f, -0.85f, 0.25f),		float3(0.972f, 0.96f, 0.915f),	0.80f, true },	// silver sphere 6
	{ 2, float4(1.6875f, 0.25f, -0.85f, 0.25f),		float3(0.972f, 0.96f, 0.915f),	1.0f, true }	// silver sphere 7

};

// =======================================================================
//
// Support functions
//
// =======================================================================

static const float A = 0.22f;
static const float B = 0.30f;
static const float C = 0.10f;
static const float D = 0.20f;
static const float E = 0.01f;
static const float F = 0.30f;
static const float W = 11.2f;

float3 Uncharted2Tonemap(float3 x)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float Random(float3 pixel, float3 scale, float seed)
{
	return frac(sin(dot(pixel + seed, scale)) * 43758.5453 + seed);
}

float3 UniformSample(float3 n, float3 pixel, float seed)
{
	float u = Random(pixel, float3(12.9898f, 78.233f, 151.7182f), seed);
	float v = Random(pixel, float3(63.7264f, 10.873f, 623.6736f), seed);

	float phi = 2 * PI * u;
	float costheta = v;
	float sintheta = sqrt(1 - costheta * costheta);

	float3 H;

	H.x = sintheta * cos(phi);
	H.y = sintheta * sin(phi);
	H.z = costheta;

	float3 up = ((abs(n.z) < 0.999) ? float3(0, 0, 1) : float3(1, 0, 0));
	float3 tangent = normalize(cross(up, n));
	float3 bitangent = cross(n, tangent);

	// PDF = 1.0 / (2 * PI);
	return tangent * H.x + bitangent * H.y + n * H.z;
}

float3 CosineSample(float3 n, float3 pixel, float seed)
{
	float u = Random(pixel, float3(12.9898f, 78.233f, 151.7182f), seed);
	float v = Random(pixel, float3(63.7264f, 10.873f, 623.6736f), seed);

	float phi = 2 * PI * u;
	float costheta = sqrt(v);
	float sintheta = sqrt(1 - costheta * costheta);

	float3 H;

	H.x = sintheta * cos(phi);
	H.y = sintheta * sin(phi);
	H.z = costheta;

	float3 up = ((abs(n.z) < 0.999) ? float3(0, 0, 1) : float3(1, 0, 0));
	float3 tangent = normalize(cross(up, n));
	float3 bitangent = cross(n, tangent);

	return tangent * H.x + bitangent * H.y + n * H.z;
}

float3 ImportanceSampleGGX(float3 n, float roughness, float3 pixel, float seed)
{
	float u = Random(pixel, float3(12.9898f, 78.233f, 151.7182f), seed);
	float v = Random(pixel, float3(63.7264f, 10.873f, 623.6736f), seed);

	float m = roughness * roughness;
	float m2 = m * m;

	float phi = 2 * PI * u;
	float costheta = sqrt((1 - v) / (1 + (m2 - 1) * v));
	float sintheta = sqrt(1 - costheta * costheta);

	float3 H;

	H.x = sintheta * cos(phi);
	H.y = sintheta * sin(phi);
	H.z = costheta;
	
	float3 up = ((abs(n.z) < 0.999) ? float3(0, 0, 1) : float3(1, 0, 0));
	float3 tangent = normalize(cross(up, n));
	float3 bitangent = cross(n, tangent);

	return tangent * H.x + bitangent * H.y + n * H.z;
}

// =======================================================================
//
// BRDF functions
//
// =======================================================================

float GGX(float ndoth, float roughness)
{
	float m = roughness * roughness;
	float m2 = m * m;
	float d = (ndoth * m2 - ndoth) * ndoth + 1.0;

	return m2 / (PI * d * d);
}

float Smith(float ndotl, float ndotv, float roughness)
{
	float a = roughness * 0.5f + 0.5f;
	float a2 = a * a * a * a;

	float lambda_v = (-1 + sqrt(a2 * (1 - ndotl * ndotl) / (ndotl * ndotl) + 1)) * 0.5f;
	float lambda_l = (-1 + sqrt(a2 * (1 - ndotv * ndotv) / (ndotv * ndotv) + 1)) * 0.5f;

	return rcp(1 + lambda_v + lambda_l);
}

float3 BRDF_Diffuse(SceneObject obj)
{
	// Lambert
	return (obj.metal ? 0 : (obj.baseColor * ONE_OVER_PI));
}

float3 BRDF_Specular_GGX(SceneObject obj, float3 l, float3 v, float3 n)
{
	// GGX sampled <=> PDF = (D * ndoth) / (4 * vdoth)
	float3 h = normalize(l + v);
	float3 f0 = (obj.metal ? obj.baseColor : 0.04f);

	float ldoth = saturate(dot(l, h));
	float ndoth = saturate(dot(n, h));
	float ndotl = saturate(dot(l, n));
	float ndotv = saturate(dot(v, n));
	float vdoth = saturate(dot(v, h));

	float3 F = f0 + (1.0f - f0) * pow(1 - ldoth, 5.0f);
	float G = Smith(ndotl, ndotv, obj.roughness);

	return max(0, (F * G * vdoth) / (ndotv * ndoth));
}

float3 BRDF_Specular_Cosine(SceneObject obj, float3 l, float3 v, float3 n)
{
	// Cosine sampled <=> PDF = costheta / PI
	float3 h = normalize(l + v);
	float3 f0 = (obj.metal ? obj.baseColor : 0.04f);

	float ldoth = saturate(dot(l, h));
	float ndoth = saturate(dot(n, h));
	float ndotl = saturate(dot(l, n));
	float ndotv = saturate(dot(v, n));

	float3 F = f0 + (1.0f - f0) * pow(1 - ldoth, 5.0f);
	float D = GGX(ndoth, obj.roughness);
	float G = Smith(ndotl, ndotv, obj.roughness);
	float d = 4 * ndotl * ndotv;

	// multiply with PI later
	return max(0, D * F * G * d);
}

// =======================================================================
//
// Path tracing functions
//
// =======================================================================

float RayIntersectPlane(out float3 n, float4 p, float3 start, float3 dir)
{
	float u = (dir.x * p.x + dir.y * p.y + dir.z * p.z);
	float t = 0;

	n = p.xyz;

	if( u < -1e-5f )
	//if( abs(u) > 1e-5f )
		t = -(start.x * p.x + start.y * p.y + start.z * p.z + p.w) / u;

	return ((t > 0.0f) ? t : FLT_MAX);
}

float RayIntersectSphere(out float3 n, float3 center, float radius, float3 start, float3 dir)
{
	float3 stoc = start - center;

	float a = dot(dir, dir);
	float b = 2.0f * dot(stoc, dir);
	float c = dot(stoc, stoc) - radius * radius;
	float d = b * b - 4.0f * a * c;
	float t = 0;

	if( d > 0.0f)
		t = (-b - sqrt(d)) / (2.0f * a);

	n = normalize(start + t * dir - center);

	return ((t > 0.0f) ? t : FLT_MAX);
}

int FindIntersection(out float3 pos, out float3 norm, float3 raystart, float3 raydir)
{
	float3	bestn, n;
	float	t, bestt	= FLT_MAX;
	int		index		= NUM_OBJECTS;
	int		i;

	// find first object that the ray hits
	[unroll]
	for( i = 0; i < NUM_OBJECTS; ++i )
	{
		[branch]
		if( objects[i].type == 1 )
			t = RayIntersectPlane(n, objects[i].params, raystart, raydir);
		else
			t = RayIntersectSphere(n, objects[i].params.xyz, objects[i].params.w, raystart, raydir);

		if( t < bestt )
		{
			bestt	= t;
			bestn	= n;
			index	= i;
		}
	}

	if( index < NUM_OBJECTS )
	{
		pos = raystart + (bestt - 1e-3f) * raydir;
		norm = bestn;
	}

	return index;
}

float3 SampleLightExplicit(out float3 shadowray, float3 p, float3 pixel)
{
	float3	ret = 0;
	float3	lp, ln;
	int		other;

	ln = normalize(p - objects[0].params.xyz); //
	lp = UniformSample(ln, pixel, time);
	lp = (objects[0].params.xyz + lp * objects[0].params.w); //

	shadowray = normalize(lp - p);
	other = FindIntersection(lp, ln, p, shadowray);

	if( other == 0 ) //
	{
		float3 v = p - lp;
		float ldotn = saturate(-dot(shadowray, ln));
		float invdistsq = rcp(dot(v, v));
		float area = 2 * PI * objects[0].params.w;

		ret = objects[0].baseColor * ldotn * invdistsq * area;
	}

	return ret;
}

float3 TraceScene(float3 raystart, float3 raydir, float3 pixel)
{
	float3	direct		= 1;
	float3	indirect	= 1;
	float3	lightrad	= 0;
	float3	fr_cos_over_pdf;
	float3	outray		= raydir;
	float3	inray;
	float3	fd, fs;
	float3	n, p		= raystart;
	float	ldotn;
	int		j, index;

	//for( j = 0; j < 1; ++j )
	[loop] for( j = 0; j < TRACE_DEPTH; ++j )
	{
		index = FindIntersection(p, n, p, outray);

		if( index >= NUM_OBJECTS )
			break;

		if( objects[index].type == 0 )
		{
			// hit light
			lightrad = objects[index].baseColor;
			break;
		}

		fd = BRDF_Diffuse(objects[index]);

		/*
		// explicit light sampling
		float3 light = SampleLightExplicit(inray, p, pixel);
		ldotn = saturate(dot(inray, n));

		direct *= (light * fd * ldotn);
		*/

		[branch]
		if( objects[index].metal )
		{
			float3 refl = outray - 2 * dot(outray, n) * n;

			inray = ImportanceSampleGGX(refl, objects[index].roughness, pixel, time + float(j));
			fs = BRDF_Specular_GGX(objects[index], inray, -outray, n);

			fr_cos_over_pdf = fs;
		}
		else
		{
			inray = CosineSample(n, pixel, time + float(j));
			//inray = UniformSample(n, pixel, time + float(j));

			fs = BRDF_Specular_Cosine(objects[index], inray, -outray, n);

			//fr_cos_over_pdf = fd * 2 * PI * ldotn; // when uniform sampling
			fr_cos_over_pdf = (fd + fs) * PI;
		}

		indirect *= fr_cos_over_pdf;
		outray = inray;
	}

	return indirect * lightrad; // + direct
}

// =======================================================================
//
// Shaders
//
// =======================================================================

static const float4 quadVertices[4] =
{
	{ -1, -1, 0.5f, 1 },
	{ -1, 1, 0.5f, 1 },
	{ 1, -1, 0.5f, 1 },
	{ 1, 1, 0.5f, 1 },
};

static const float2 subpixels[4] =
{
	{ -0.5f, -0.5f },
	{ -0.5f, 0.5f },
	{ 0.5, -0.5f },
	{ 0.5f, 0.5f }
};

void vs_common(
	in		uint id		: SV_VertexID,
	out		float4 opos	: SV_Position)
{
	opos = quadVertices[id];
}

void ps_pathtrace(
	in	float4 spos		: SV_Position,
	out	float4 color	: SV_Target)
{
	float4 ndc = float4(0, 0, 0.1f, 1);
	float4 wpos;
	float3 raydir;

	float3 prev = prevIteration.Sample(pointSampler, spos.xy / screenSize).rgb;
	float3 curr = 0;
	float d = rcp(currSample);

	// SSAA
	//[unroll]
	//for( int i = 0; i < 4; ++i )
	//{
		// calculate ray
		//ndc.xy = ((spos.xy + subpixels[i]) / screenSize) * float2(2, -2) + float2(-1, 1);
		ndc.xy = (spos.xy / screenSize) * float2(2, -2) + float2(-1, 1);

		wpos = mul(ndc, matViewProjInv);
		wpos /= wpos.w;

		raydir = normalize(wpos.xyz - eyePos);

		// trace scene
		curr += TraceScene(eyePos, raydir, spos.xyz);
	//}

	//curr *= 0.25f;

	color.rgb = prev * (1.0f - d) + curr * d;
	color.a = 1;
}

void ps_tonemap(
	in	float4 spos		: SV_Position,
	out	float4 color	: SV_Target)
{
	const float exposure = 8.0f; // TODO: auto

	float2 ndc = spos.xy / screenSize;

	float4 base = sceneTexture.Sample(linearSampler, ndc);
	float3 lincolor = Uncharted2Tonemap(base.rgb * exposure);
	float3 invlinwhite = rcp(Uncharted2Tonemap(W));

	// NOTE: assume backbuffer is srgb
	color.rgb = lincolor * invlinwhite;
	//color.rgb = base.rgb;
	color.a = 1;
}

// =======================================================================
//
// Techniques
//
// =======================================================================

technique10 pathtrace
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_common()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, ps_pathtrace()));
	}
}

technique10 tonemap
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_common()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, ps_tonemap()));
	}
}
