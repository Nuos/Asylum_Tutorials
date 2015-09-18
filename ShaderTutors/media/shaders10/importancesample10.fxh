
// as described in http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

#define PI	3.1415926535897932

uint ReverseBits32(uint bits)
{
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);

	return bits;
}

float2 Hammersley(uint Index, uint NumSamples)
{
	float E1 = frac(float(Index) / float(NumSamples));
	float E2 = float(ReverseBits32(Index)) * 2.3283064365386963e-10;
	
	return float2(E1, E2);
}

float2 Random(float3 pixel, float seed)
{
	const float3 scale1 = { 12.9898f, 78.233f, 151.7182f };
	const float3 scale2 = { 63.7264f, 10.873f, 623.6736f };

	float2 Xi;
	
	Xi.x = frac(sin(dot(pixel + seed, scale1)) * 43758.5453 + seed);
	Xi.y = frac(sin(dot(pixel + seed, scale2)) * 43758.5453 + seed);

	return Xi;
}

float3 CosineSample(float2 Xi)
{
	float phi = 2 * PI * Xi.x;
	float costheta = sqrt(Xi.y);
	float sintheta = sqrt(1 - costheta * costheta);

	return float3(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

float3 GGXSample(float2 Xi, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;

	float phi = 2 * PI * Xi.x;
	float costheta = sqrt((1 - Xi.y) / (1 + (a2 - 1) * Xi.y));
	float sintheta = sqrt(1 - costheta * costheta);

	return float3(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

float3 TangentToWorld(float3 Vec, float3 TangentZ)
{
	float3 UpVector = abs(TangentZ.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, TangentZ));
	float3 TangentY = cross(TangentZ, TangentX);

	return TangentX * Vec.x + TangentY * Vec.y + TangentZ * Vec.z;
}
