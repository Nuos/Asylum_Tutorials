

#define NUM_MIPS		8
#define ONE_OVER_PI		0.3183098861837906

uniform TextureCube irradCubeDiff;
uniform TextureCube irradCubeSpec;
uniform Texture2D brdfLUT;

uniform matrix matWorld;
uniform matrix matWorldInv;
uniform matrix matViewProj;

uniform float3 eyePos;
uniform float3 baseColor = { 1.022f, 0.782f, 0.344f };

uniform float roughness = 0.3f;
uniform float metalness = 1.0f;

SamplerState linearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

void vs_metal(
	in		float3 pos		: POSITION,
	in		float3 norm		: NORMAL,
	out		float3 wnorm	: TEXCOORD0,
	out		float3 vdir		: TEXCOORD1,
	out		float4 opos		: SV_Position)
{
	float4 wpos = mul(float4(pos, 1), matWorld);
	wnorm = mul(matWorldInv, float4(norm, 0)).xyz;

	vdir = eyePos - wpos.xyz;
	opos = mul(wpos, matViewProj);
}

void ps_metal(
	in	float3 wnorm	: TEXCOORD0,
	in	float3 vdir		: TEXCOORD1,
	out	float4 color	: SV_Target)
{
	float3 n = normalize(wnorm);
	float3 v = normalize(vdir);

	float miplevel = roughness * (NUM_MIPS - 1);

	float3 r = 2 * dot(v, n) * n - v;

	float3 diff_irrad = irradCubeDiff.SampleLevel(linearSampler, n, 0).rgb;
	float3 spec_irrad = irradCubeSpec.SampleLevel(linearSampler, r, miplevel).rgb;

	float ndotv = saturate(dot(n, v));
	float2 AB = brdfLUT.Sample(linearSampler, float2(ndotv, roughness)).rg;

	float3 F0 = lerp(0.04f, baseColor, metalness);

	float3 fd = lerp(diff_irrad * baseColor * ONE_OVER_PI, 0.0f, metalness);
	float3 fs = spec_irrad * (F0 * AB.x + AB.y);

	color.rgb = fd + fs;
	color.a = 1.0f;
}

technique10 metal
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_metal()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, ps_metal()));
	}
}
