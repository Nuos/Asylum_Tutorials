
sampler mytex0 : register(s0) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = linear;
};

matrix matWorld;
matrix matViewProj;

float4 ambient = { 1, 1, 1, 1 };
float2 uv = { 1, 1 };

void vs_main(
	in float3 pos	: POSITION,
	out float4 opos : POSITION,
	in out float2 tex	: TEXCOORD0)
{
	opos = mul(float4(pos, 1), matWorld);
	opos = mul(opos, matViewProj);

	tex *= uv;
}

void ps_main(
	in	float2 tex		: TEXCOORD0,
	out	float4 color	: COLOR0)
{
	color = tex2D(mytex0, tex);
	color.rgb *= ambient.rgb;
}

technique ambientlight
{
	pass p0
	{
		vertexshader = compile vs_2_0 vs_main();
		pixelshader = compile ps_2_0 ps_main();
	}
}
