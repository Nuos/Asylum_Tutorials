
#include "importancesample10.fxh"

#define NUM_SAMPLES		2048

static const float4 quadVertices[4] =
{
	{ -1, -1, 0.5f, 1 },
	{ -1, 1, 0.5f, 1 },
	{ 1, -1, 0.5f, 1 },
	{ 1, 1, 0.5f, 1 },
};

float G_Smith(float ndotl, float ndotv, float roughness)
{
	float a = roughness * 0.5f + 0.5f;
	float a2 = a * a * a * a;

	float lambda_v = (-1 + sqrt(a2 * (1 - ndotl * ndotl) / (ndotl * ndotl) + 1)) * 0.5f;
	float lambda_l = (-1 + sqrt(a2 * (1 - ndotv * ndotv) / (ndotv * ndotv) + 1)) * 0.5f;

	return rcp(1 + lambda_v + lambda_l);
}

float2 IntegrateBRDF(float roughness, float NoV)
{
	float3 N = { 0.0f, 0.0f, 1.0f };
	float3 V = { sqrt(1.0f - NoV * NoV), 0.0f, NoV };
	float3 H, L;
	float2 Xi;
	float2 AB = 0;

	for( uint i = 0; i < NUM_SAMPLES; ++i )
	{
		Xi = Hammersley(i, NUM_SAMPLES);
		H = GGXSample(Xi, roughness);

		L = 2.0f * dot(V, H) * H - V;

		float NoL = saturate(L.z);
		float NoH = saturate(H.z);
		float VoH = saturate(dot(V, H));

		if( NoL > 0 )
		{
			float Fc = pow(1 - VoH, 5.0f);

			// PDF = (D * NoH) / (4 * VoH)
			float G = G_Smith(NoL, NoV, roughness);
			float G_mul_pdf = saturate((G * VoH) / (NoV * NoH));

			AB.x += (1 - Fc) * G_mul_pdf;
			AB.y += Fc * G_mul_pdf;
		}
	}

	AB.x /= (float)NUM_SAMPLES;
	AB.y /= (float)NUM_SAMPLES;

	return AB;
}

void vs_integratebrdf(
	in		uint id		: SV_VertexID,
	out		float4 opos	: SV_Position)
{
	opos = quadVertices[id];
}

void ps_integratebrdf(
	in	float4 spos		: SV_Position,
	out	float4 color	: SV_Target)
{
	float2 ndc = spos.xy / 256.0f;

	color.rg = IntegrateBRDF(ndc.y, ndc.x);
	color.ba = float2(0, 1);
}

technique10 integratebrdf
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, vs_integratebrdf()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, ps_integratebrdf()));
	}
}
