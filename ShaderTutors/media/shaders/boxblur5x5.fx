
sampler2D sampler0 : register(s0) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = none;
};

uniform float2 texelSize;

void ps_blur(
	in	float2 tex		: TEXCOORD0,
	out	float4 color0	: COLOR0)
{
	color0 = 0;

	[unroll]
	for( int i = -2; i < 3; ++i )
		for( int j = -2; j < 3; ++j )
			color0 += tex2D(sampler0, tex + texelSize * float2(i, j));

	color0 *= 0.04f;
}

technique blur
{
	pass p0
	{
		vertexshader = null;
		pixelshader = compile ps_2_0 ps_blur();
	}
}
