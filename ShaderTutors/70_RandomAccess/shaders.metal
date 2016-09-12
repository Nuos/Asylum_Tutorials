//
//  shaders.metal
//  70_Metal
//
//  Created by iszennai on 22/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#include <metal_stdlib>

using namespace metal;

struct CommonVertex
{
	packed_float3 pos;
	uint norm;
	packed_half2 tex;
	ushort matID;
};

struct ConstantBuffer
{
	float4x4 matWorld;
	float4x4 matViewProj;
	float4 lightPos;
	float4 eyePos;
};

struct VS_OUTPUT
{
	float4 pos [[position]];
	float3 wnorm;
	float3 vdir;
	float3 ldir;
	float2 tex;
	ushort matID;
};

vertex VS_OUTPUT vs_main(
	device CommonVertex* vertices [[buffer(0)]],
	constant ConstantBuffer& uniforms [[buffer(1)]],
	unsigned int vid [[vertex_id]])
{
	VS_OUTPUT out;
	
	float4 pos = float4(float3(vertices[vid].pos), 1.0);
	float4 norm = unpack_unorm4x8_to_float(vertices[vid].norm);
	float4 wpos = uniforms.matWorld * pos;
	
	norm.xyz = norm.xyz * 2.0 - 1.0;
	
	out.pos = uniforms.matViewProj * wpos;
	out.wnorm = (uniforms.matWorld * norm).xyz;
	out.vdir = uniforms.eyePos.xyz - wpos.xyz;
	out.ldir = uniforms.lightPos.xyz - wpos.xyz;
	out.tex = float2(vertices[vid].tex);
	out.matID = vertices[vid].matID;
	
	return out;
}


fragment half4 ps_main(
	VS_OUTPUT in [[stage_in]],
	texture2d<half> tex0 [[texture(0)]],
	constant float4* materials [[buffer(0)]])
{
	constexpr sampler sampler0(coord::normalized, address::repeat, filter::linear);
	
	float3 n = normalize(in.wnorm);
	float3 l = normalize(in.ldir);
	float3 v = normalize(in.vdir);
	float3 h = normalize(l + v);
	
	half d = saturate(dot(n, l));
	half s = pow(saturate(dot(n, h)), 80.0);

	half4 base = tex0.sample(sampler0, in.tex);
	base.rgb = base.rgb * half3(materials[in.matID].rgb) * d + half3(s);

	return base;
}

kernel void coloredgrid(
	texture2d<half, access::write> tex0 [[texture(0)]],
	uint2 loc [[thread_position_in_grid]],
	constant float& time [[buffer(0)]])
{
	half4 color = { 0.0, 0.0, 0.0, 1.0 };
	
	if ((loc.x / 16 + loc.y / 16) % 2 == 1) {
		color.r = 1.0; //sin(time) * 0.5 + 0.5;
		color.g = 1.0; //cos(time) * 0.5 + 0.5;
		color.b = 1.0; //sin(time) * cos(time) * 0.5 + 0.5;
	}
	
	tex0.write(color, loc);
}

