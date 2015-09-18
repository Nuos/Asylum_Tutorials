
uniform TextureCube envmap;

uniform matrix matWorld;
uniform matrix matViewProj;

uniform float3 eyePos;
uniform float gamma;

SamplerState linearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

void vs_sky(
	in		float3 pos	: POSITION,
	out		float3 view	: TEXCOORD0,
	out		float4 opos	: SV_Position)
{
	float4 wpos = mul(float4(pos, 1), matWorld);

	view = wpos.xyz - eyePos;
	opos = mul(wpos, matViewProj);
}

void ps_sky(
	in	float3 view		: TEXCOORD0,
	out	float4 color	: SV_Target)
{
	color = envmap.Sample(linearSampler, view);
}

technique10 sky
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_sky()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, ps_sky()));
	}
}
