
#include "importancesample10.fxh"

#define NUM_DIFF_SAMPLES		16384
#define NUM_SPEC_SAMPLES		2048

uniform TextureCube envmap;

uniform matrix matWorld;
uniform matrix matProj;
uniform matrix matCubeViews[6];

uniform float3 eyePos;
uniform float roughness = 0.1f;

SamplerState linearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct GS_Input
{
	float4 pos : SV_Position;
	float3 view : TEXCOORD0;
};

struct GS_Output
{
	float4 pos : SV_Position;
	float3 view : TEXCOORD0;
	uint rtindex : SV_RenderTargetArrayIndex;
};

void vs_prefilter(
	in		float3 pos	: POSITION,
	out		GS_Input output)
{
	float4 wpos = mul(float4(pos, 1), matWorld);
	
	output.view = wpos.xyz - eyePos;
	output.pos = wpos;
}

[maxvertexcount(18)]
void gs_prefilter(
	in triangle GS_Input input[3],
	in out TriangleStream<GS_Output> stream)
{
	GS_Output output;

	for( uint i = 0; i < 6; ++i )
	{
		output.rtindex = i;

		output.pos = mul(mul(input[0].pos, matCubeViews[i]), matProj);
		output.view = input[0].view;

		stream.Append(output);

		output.pos = mul(mul(input[1].pos, matCubeViews[i]), matProj);
		output.view = input[1].view;

		stream.Append(output);

		output.pos = mul(mul(input[2].pos, matCubeViews[i]), matProj);
		output.view = input[2].view;

		stream.Append(output);
		stream.RestartStrip();
	}
}

float3 PrefilterEnvMapDiffuse(float3 N, float3 pixel)
{
	float3 color = 0;

	[loop]
	for( uint i = 0; i < NUM_DIFF_SAMPLES; ++i )
	{
		float2 E = Random(pixel, float(i)); //Hammersley(i, NUM_DIFF_SAMPLES);
		float3 L = TangentToWorld(CosineSample(E), N);

		color += envmap.SampleLevel(linearSampler, L, 0).rgb;
	}

	return color / NUM_DIFF_SAMPLES;
}

float3 PrefilterEnvMapSpecular(float roughness, float3 R)
{
	float3 color = 0;
	float weight = 0;

	for( uint i = 0; i < NUM_SPEC_SAMPLES; ++i )
	{
		float2 E = Hammersley(i, NUM_SPEC_SAMPLES);
		float3 H = TangentToWorld(GGXSample(E, roughness), R);
		float3 L = 2 * dot(R, H) * H - R;
		
		float ndotl = saturate(dot(R, L));

		if( ndotl > 0 )
		{
			color += envmap.SampleLevel(linearSampler, L, 0).rgb * ndotl;
			weight += ndotl;
		}
	}

	return color / max(weight, 0.001f);
}

void ps_prefilter_diff(
	in	GS_Output input,
	out	float4	color	: SV_Target)
{
	float3 v = normalize(input.view);

	color.rgb = PrefilterEnvMapDiffuse(v, input.pos.xyz);
	color.a = 1.0f;
}

void ps_prefilter_spec(
	in	GS_Output input,
	out	float4	color	: SV_Target)
{
	float3 v = normalize(input.view);

	color.rgb = PrefilterEnvMapSpecular(roughness, v);
	color.a = 1.0f;
}

technique10 prefilter_diff
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_prefilter()));
		SetGeometryShader(CompileShader(gs_4_0, gs_prefilter()));
		SetPixelShader(CompileShader(ps_4_0, ps_prefilter_diff()));
	}
}

technique10 prefilter_spec
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_prefilter()));
		SetGeometryShader(CompileShader(gs_4_0, gs_prefilter()));
		SetPixelShader(CompileShader(ps_4_0, ps_prefilter_spec()));
	}
}
