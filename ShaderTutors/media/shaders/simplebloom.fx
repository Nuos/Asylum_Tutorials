
sampler mytex0 : register(s0) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = point;

	AddressU = clamp;
	AddressV = clamp;
};

sampler mytex1 : register(s1) = sampler_state
{
	MinFilter = linear;
	MagFilter = linear;
	MipFilter = point;

	AddressU = clamp;
	AddressV = clamp;
};

uniform float2 texelSize;

void ps_downsample(
	in	float2 tex		: TEXCOORD0,
	out	float4 color	: COLOR0)
{
	color = 0;

	[unroll]
	for( int i = -1; i < 2; ++i )
		for( int j = -1; j < 2; ++j )
			color += tex2D(mytex0, tex + texelSize * float2(i, j));

	color /= 9.0f;
	color.a = 1;
}

void ps_blur(
	in	float2 tex		: TEXCOORD0,
	out	float4 color	: COLOR0)
{
	color = 0;

	[unroll]
	for( int i = -2; i < 3; ++i )
		for( int j = -2; j < 3; ++j )
			color += tex2D(mytex0, tex + texelSize * float2(i, j));

	color *= 0.04f;
	color.a = 1;
}

void ps_combine(
	in	float2 tex		: TEXCOORD0,
	out	float4 color	: COLOR0)
{
	float4 base = tex2D(mytex0, tex);
	float4 blur = tex2D(mytex1, tex);

	color = base + blur;
	color.a = 1;
}

void ps_gammacorrect(
	in	float2 tex		: TEXCOORD0,
	out	float4 color	: COLOR0)
{
	float4 base = tex2D(mytex0, tex);

	color.rgb = pow(base.rgb, 0.45f);
	color.a = 1;
}

technique downsample
{
	pass p0
	{
		vertexshader = null;
		pixelshader = compile ps_2_0 ps_downsample();
	}
}

technique blur
{
	pass p0
	{
		vertexshader = null;
		pixelshader = compile ps_2_0 ps_blur();
	}
}

technique combine
{
	pass p0
	{
		vertexshader = null;
		pixelshader = compile ps_2_0 ps_combine();
	}
}

technique gammacorrect
{
	pass p0
	{
		vertexshader = null;
		pixelshader = compile ps_2_0 ps_gammacorrect();
	}
}
