
#define NUM_SAMPLES 23

uniform float4 lightColor = { 1.0f, 1.0f, 0.7f, 1 };
uniform float3 lightPos;	// screen space
uniform float exposure;
uniform float2 texelSize;

uniform float density	= 0.8f;
uniform float decay		= 1.0f;
uniform float weight	= 1.0f;

uniform sampler2D sampler0 : register(s0) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = point;

	AddressU = clamp;
	AddressV = clamp;
};

uniform sampler2D sampler1 : register(s1) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = point;

	AddressU = clamp;
	AddressV = clamp;
};

uniform sampler2D sampler2 : register(s2) = sampler_state // bloom (if given)
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = point;

	AddressU = clamp;
	AddressV = clamp;
};

float4 AdjustSaturation(float4 color, float saturation)
{
	float grey = dot(color.rgb, float3(0.3f, 0.59f, 0.11f));
	return lerp(grey, color, saturation);
}

void ps_godray(
	in	float2 tex		: TEXCOORD0,
	out	float4 color0	: COLOR0)
{
	float2 delta = (tex - lightPos.xy);
	float color = tex2D(sampler0, tex).a;
	float sample;

	// attenuation
	float illum = 1.0f;

	// sampling density
	delta *= density / NUM_SAMPLES;

	for( int i = 0; i < NUM_SAMPLES; ++i )
	{
		tex -= delta;

		sample = tex2D(sampler0, tex).a;
		sample *= (illum * weight);

		color += sample;
		illum *= decay;
	}

	color /= NUM_SAMPLES;
	color *= exposure;

	color0 = float4(color, color, color, color);
}

void ps_blur(
	in	float2 tex		: TEXCOORD0,
	out	float4 color0	: COLOR0)
{
	float2 dist = tex - lightPos.xy;
	float off = dot(dist, dist) * 2;
	float weight = lerp(2, 8, sqrt(off));
	float2 delta = normalize(dist) * texelSize * weight;

	color0 = 0;

	for( int i = 0; i <= NUM_SAMPLES; ++i )
		color0 += tex2D(sampler0, tex - i * delta);

	color0 /= NUM_SAMPLES;
}

void ps_final(
	in	float2 tex		: TEXCOORD0,
	out	float4 color0	: COLOR0)
{
	float4	base	= tex2D(sampler0, tex);
	float4	bloom	= tex2D(sampler2, tex);
	float	shaft	= tex2D(sampler1, tex).a;

	tex -= 0.5f;

	shaft = saturate(pow(shaft, 1.8f) * 2.5f);
	bloom = AdjustSaturation(bloom, 0.4f);

	float4 rays = shaft * lightColor;
	float v = 1 - dot(tex, tex);

	rays *= v;
	bloom *= 0.5f;

	v *= v;

	color0 = (base + bloom) * v + rays;
}

technique godray
{
	pass p0
	{
		pixelshader = compile ps_2_0 ps_godray();
	}
}

technique blur
{
	pass p0
	{
		pixelshader = compile ps_2_0 ps_blur();
	}
}

technique final
{
	pass p0
	{
		pixelshader = compile ps_2_0 ps_final();
	}
}

