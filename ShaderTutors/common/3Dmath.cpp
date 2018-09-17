
#include "3Dmath.h"
#include <cstring>

// *****************************************************************************************************************************
//
// Color impl
//
// *****************************************************************************************************************************

CLASS_PROTO(Color)::CLASS_PROTO(Color)()
	: r(0), g(0), b(0), a(1)
{
}

CLASS_PROTO(Color)::CLASS_PROTO(Color)(float _r, float _g, float _b, float _a)
	: r(_r), g(_g), b(_b), a(_a)
{
}

CLASS_PROTO(Color)::CLASS_PROTO(Color)(uint32_t argb32)
{
	a = ArgbA32(argb32) / 255.0f;
	r = ArgbR32(argb32) / 255.0f;
	g = ArgbG32(argb32) / 255.0f;
	b = ArgbB32(argb32) / 255.0f;
}

CLASS_PROTO(Color) CLASS_PROTO(Color)::Lerp(const CLASS_PROTO(Color)& from, const CLASS_PROTO(Color)& to, float frac)
{
	CLASS_PROTO(Color) ret;

	ret.r = from.r * (1 - frac) + to.r * frac;
	ret.g = from.g * (1 - frac) + to.g * frac;
	ret.b = from.b * (1 - frac) + to.b * frac;
	ret.a = from.a * (1 - frac) + to.a * frac;

	return ret;
}

CLASS_PROTO(Color) CLASS_PROTO(Color)::sRGBToLinear(uint8_t red, uint8_t green, uint8_t blue)
{
	CLASS_PROTO(Color) ret;

	float lo_r = (float)red / 3294.6f;
	float lo_g = (float)green / 3294.6f;
	float lo_b = (float)blue / 3294.6f;

	float hi_r = powf((red / 255.0f + 0.055f) / 1.055f, 2.4f);
	float hi_g = powf((green / 255.0f + 0.055f) / 1.055f, 2.4f);
	float hi_b = powf((blue / 255.0f + 0.055f) / 1.055f, 2.4f);

	ret.r = (red < 10 ? lo_r : hi_r);
	ret.g = (green < 10 ? lo_g : hi_g);
	ret.b = (blue < 10 ? lo_b : hi_b);
	ret.a = 1;

	return ret;
}

// *****************************************************************************************************************************
//
// AABox impl
//
// *****************************************************************************************************************************

#define ASGN_IF(a, op, b) \
	if( (a) op (b) ) (a) = (b);

CLASS_PROTO(AABox)::CLASS_PROTO(AABox)()
{
	Min[0] = Min[1] = Min[2] = FLT_MAX;
	Max[0] = Max[1] = Max[2] = -FLT_MAX;
}

CLASS_PROTO(AABox)::CLASS_PROTO(AABox)(const CLASS_PROTO(AABox)& other)
{
	operator =(other);
}

CLASS_PROTO(AABox)::CLASS_PROTO(AABox)(const float size[3])
{
	FUNC_PROTO(Vec3Set)(Min, size[0] * -0.5f, size[1] * -0.5f, size[2] * -0.5f);
	FUNC_PROTO(Vec3Set)(Max, size[0] * 0.5f, size[1] * 0.5f, size[2] * 0.5f);
}

CLASS_PROTO(AABox)::CLASS_PROTO(AABox)(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax)
{
	FUNC_PROTO(Vec3Set)(Min, xmin, ymin, zmin);
	FUNC_PROTO(Vec3Set)(Max, xmax, ymax, zmax);
}

CLASS_PROTO(AABox)& CLASS_PROTO(AABox)::operator =(const CLASS_PROTO(AABox)& other)
{
	if( &other != this )
	{
		Min[0] = other.Min[0];
		Min[1] = other.Min[1];
		Min[2] = other.Min[2];

		Max[0] = other.Max[0];
		Max[1] = other.Max[1];
		Max[2] = other.Max[2];
	}

	return *this;
}

bool CLASS_PROTO(AABox)::Intersects(const CLASS_PROTO(AABox)& other) const
{
	if( Max[0] < other.Min[0] || Min[0] > other.Max[0] )
		return false;

	if( Max[1] < other.Min[1] || Min[1] > other.Max[1] )
		return false;

	if( Max[2] < other.Min[2] || Min[2] > other.Max[2] )
		return false;

	return true;
}

void CLASS_PROTO(AABox)::Add(float x, float y, float z)
{
	ASGN_IF(Max[0], <, x);
	ASGN_IF(Max[1], <, y);
	ASGN_IF(Max[2], <, z);

	ASGN_IF(Min[0], >, x);
	ASGN_IF(Min[1], >, y);
	ASGN_IF(Min[2], >, z);
}

void CLASS_PROTO(AABox)::Add(const float v[3])
{
	ASGN_IF(Max[0], <, v[0]);
	ASGN_IF(Max[1], <, v[1]);
	ASGN_IF(Max[2], <, v[2]);

	ASGN_IF(Min[0], >, v[0]);
	ASGN_IF(Min[1], >, v[1]);
	ASGN_IF(Min[2], >, v[2]);
}

void CLASS_PROTO(AABox)::GetCenter(float out[3]) const
{
	out[0] = (Min[0] + Max[0]) * 0.5f;
	out[1] = (Min[1] + Max[1]) * 0.5f;
	out[2] = (Min[2] + Max[2]) * 0.5f;
}

void CLASS_PROTO(AABox)::GetSize(float out[3]) const
{
	out[0] = Max[0] - Min[0];
	out[1] = Max[1] - Min[1];
	out[2] = Max[2] - Min[2];
}

void CLASS_PROTO(AABox)::GetHalfSize(float out[3]) const
{
	out[0] = (Max[0] - Min[0]) * 0.5f;
	out[1] = (Max[1] - Min[1]) * 0.5f;
	out[2] = (Max[2] - Min[2]) * 0.5f;
}

void CLASS_PROTO(AABox)::Inset(float dx, float dy, float dz)
{
	Min[0] += dx;
	Min[1] += dy;
	Min[2] += dz;

	Max[0] -= dx;
	Max[1] -= dy;
	Max[2] -= dz;
}

void CLASS_PROTO(AABox)::TransformAxisAligned(const float traf[16])
{
	float vertices[8][3] =
	{
		{ Min[0], Min[1], Min[2] },
		{ Min[0], Min[1], Max[2] },
		{ Min[0], Max[1], Min[2] },
		{ Min[0], Max[1], Max[2] },
		{ Max[0], Min[1], Min[2] },
		{ Max[0], Min[1], Max[2] },
		{ Max[0], Max[1], Min[2] },
		{ Max[0], Max[1], Max[2] }
	};
	
	for( int i = 0; i < 8; ++i )
		FUNC_PROTO(Vec3TransformCoord)(vertices[i], vertices[i], traf);

	Min[0] = Min[1] = Min[2] = FLT_MAX;
	Max[0] = Max[1] = Max[2] = -FLT_MAX;

	for( int i = 0; i < 8; ++i )
		Add(vertices[i]);
}

void CLASS_PROTO(AABox)::GetPlanes(float outplanes[6][4]) const
{
#define CALC_PLANE(i, nx, ny, nz, px, py, pz) \
	outplanes[i][0] = nx;	p[0] = px; \
	outplanes[i][1] = ny;	p[1] = py; \
	outplanes[i][2] = nz;	p[2] = pz; \
	outplanes[i][3] = -FUNC_PROTO(Vec3Dot)(p, outplanes[i]); \
	FUNC_PROTO(PlaneNormalize)(outplanes[i], outplanes[i]);
// END

	float p[3];

	CALC_PLANE(0, 1, 0, 0, Min[0], Min[1], Min[2]);		// left
	CALC_PLANE(1, -1, 0, 0, Max[0], Min[1], Min[2]);	// right
	CALC_PLANE(2, 0, 1, 0, Min[0], Min[1], Min[2]);		// bottom
	CALC_PLANE(3, 0, -1, 0, Min[0], Max[1], Min[2]);	// top
	CALC_PLANE(4, 0, 0, -1, Min[0], Min[1], Max[2]);	// front
	CALC_PLANE(5, 0, 0, 1, Min[0], Min[1], Min[2]);		// back
}

float CLASS_PROTO(AABox)::Radius() const
{
	return FUNC_PROTO(Vec3Distance)(Min, Max) * 0.5f;
}

float CLASS_PROTO(AABox)::RayIntersect(const float start[3], const float dir[3]) const
{
	float m1[3], m2[3];
	float t1, t2, t3, t4, t5, t6;

	FUNC_PROTO(Vec3Subtract)(m1, Min, start);
	FUNC_PROTO(Vec3Subtract)(m2, Max, start);

	if( dir[0] == 0 ) {
		t1 = (m1[0] >= 0 ? FLT_MAX : -FLT_MAX);
		t2 = (m2[0] >= 0 ? FLT_MAX : -FLT_MAX);
	} else {
		t1 = m1[0] / dir[0];
		t2 = m2[0] / dir[0];
	}

	if( dir[1] == 0 ) {
		t3 = (m1[1] >= 0 ? FLT_MAX : -FLT_MAX);
		t4 = (m2[1] >= 0 ? FLT_MAX : -FLT_MAX);
	} else {
		t3 = m1[1] / dir[1];
		t4 = m2[1] / dir[1];
	}

	if( dir[2] == 0 ) {
		t5 = (m1[2] >= 0 ? FLT_MAX : -FLT_MAX);
		t6 = (m2[2] >= 0 ? FLT_MAX : -FLT_MAX);
	} else {
		t5 = m1[2] / dir[2];
		t6 = m2[2] / dir[2];
	}

	float tmin = FUNC_PROTO(Max)(FUNC_PROTO(Max)(FUNC_PROTO(Min)(t1, t2), FUNC_PROTO(Min)(t3, t4)), FUNC_PROTO(Min)(t5, t6));
	float tmax = FUNC_PROTO(Min)(FUNC_PROTO(Min)(FUNC_PROTO(Max)(t1, t2), FUNC_PROTO(Max)(t3, t4)), FUNC_PROTO(Max)(t5, t6));

	if( tmax < 0 || tmin > tmax )
		return FLT_MAX;

	return tmin;
}

float CLASS_PROTO(AABox)::Nearest(float from[4]) const
{
#define FAST_DISTANCE(x, y, z, p, op) \
	d = p[0] * x + p[1] * y + p[2] * z + p[3]; \
	ASGN_IF(dist, op, d);
// END

	float d, dist = FLT_MAX;

	FAST_DISTANCE(Min[0], Min[1], Min[2], from, >);
	FAST_DISTANCE(Min[0], Min[1], Max[2], from, >);
	FAST_DISTANCE(Min[0], Max[1], Min[2], from, >);
	FAST_DISTANCE(Min[0], Max[1], Max[2], from, >);
	FAST_DISTANCE(Max[0], Min[1], Min[2], from, >);
	FAST_DISTANCE(Max[0], Min[1], Max[2], from, >);
	FAST_DISTANCE(Max[0], Max[1], Min[2], from, >);
	FAST_DISTANCE(Max[0], Max[1], Max[2], from, >);

	return dist;
}

float CLASS_PROTO(AABox)::Farthest(float from[4]) const
{
	float d, dist = -FLT_MAX;

	FAST_DISTANCE(Min[0], Min[1], Min[2], from, <);
	FAST_DISTANCE(Min[0], Min[1], Max[2], from, <);
	FAST_DISTANCE(Min[0], Max[1], Min[2], from, <);
	FAST_DISTANCE(Min[0], Max[1], Max[2], from, <);
	FAST_DISTANCE(Max[0], Min[1], Min[2], from, <);
	FAST_DISTANCE(Max[0], Min[1], Max[2], from, <);
	FAST_DISTANCE(Max[0], Max[1], Min[2], from, <);
	FAST_DISTANCE(Max[0], Max[1], Max[2], from, <);

	return dist;
}

// *****************************************************************************************************************************
//
// Math functions impl
//
// *****************************************************************************************************************************

int32_t FUNC_PROTO(ISqrt)(int32_t n)
{
	int32_t b = 0;

	while( n >= 0 ) {
		n = n - b;
		b = b + 1;
		n = n - b;
	}

	return b - 1;
}

uint32_t FUNC_PROTO(NextPow2)(uint32_t x)
{
	--x;

	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return ++x;
}

uint32_t FUNC_PROTO(Log2OfPow2)(uint32_t x)
{
	uint32_t ret = 0;

	while( x >>= 1 )
		++ret;

	return ret;
}

uint32_t FUNC_PROTO(ReverseBits32)(uint32_t bits)
{
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);

	return bits;
}

float FUNC_PROTO(Vec2Dot)(const float a[2], const float b[2])
{
	return (a[0] * b[0] + a[1] * b[1]);
}

float FUNC_PROTO(Vec2Length)(const float a[2])
{
	return sqrtf(FUNC_PROTO(Vec2Dot)(a, a));
}

float FUNC_PROTO(Vec3Dot)(const float a[3], const float b[3])
{
	return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
}

float FUNC_PROTO(Vec3Length)(const float a[3])
{
	return sqrtf(FUNC_PROTO(Vec3Dot)(a, a));
}

float FUNC_PROTO(Vec3Distance)(const float a[3], const float b[3])
{
	float c[3] =
	{
		a[0] - b[0],
		a[1] - b[1],
		a[2] - b[2]
	};

	return FUNC_PROTO(Vec3Length)(c);
}

float FUNC_PROTO(Vec4Dot)(const float a[4], const float b[4])
{
	return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
}

float FUNC_PROTO(PlaneDistance)(const float p[4], const float v[3])
{
	return p[0] * v[0] + p[1] * v[1] + p[2] * v[2] + p[3];
}

void FUNC_PROTO(Vec2Assign)(float out[2], const float a[2])
{
	out[0] = a[0];
	out[1] = a[1];
}

void FUNC_PROTO(Vec2Set)(float out[2], float x, float y)
{
	out[0] = x;
	out[1] = y;
}

void FUNC_PROTO(Vec2Normalize)(float out[2], const float v[2])
{
	float il = 1.0f / FUNC_PROTO(Vec2Length)(v);

	out[0] = v[0] * il;
	out[1] = v[1] * il;
}

void FUNC_PROTO(Vec3Assign)(float out[3], const float a[3])
{
	out[0] = a[0];
	out[1] = a[1];
	out[2] = a[2];
}

void FUNC_PROTO(Vec3Set)(float out[3], float x, float y, float z)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

void FUNC_PROTO(Vec3Add)(float out[3], const float a[3], const float b[3])
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

void FUNC_PROTO(Vec3Mad)(float out[3], const float a[3], const float b[3], float s)
{
	out[0] = a[0] + b[0] * s;
	out[1] = a[1] + b[1] * s;
	out[2] = a[2] + b[2] * s;
}

void FUNC_PROTO(Vec3Subtract)(float out[3], const float a[3], const float b[3])
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

void FUNC_PROTO(Vec3Scale)(float out[3], const float a[3], float scale)
{
	out[0] = a[0] * scale;
	out[1] = a[1] * scale;
	out[2] = a[2] * scale;
}

void FUNC_PROTO(Vec3Modulate)(float out[3], const float a[3], const float b[3])
{
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
}

void FUNC_PROTO(Vec3Swap)(float a[3], float b[3])
{
	float tmp[3];

	FUNC_PROTO(Vec3Set)(tmp, a[0], a[1], a[2]);
	FUNC_PROTO(Vec3Set)(a, b[0], b[1], b[2]);
	FUNC_PROTO(Vec3Set)(b, tmp[0], tmp[1], tmp[2]);
}

void FUNC_PROTO(Vec3Normalize)(float out[3], const float v[3])
{
	float il = 1.0f / FUNC_PROTO(Vec3Length)(v);

	out[0] = v[0] * il;
	out[1] = v[1] * il;
	out[2] = v[2] * il;
}

void FUNC_PROTO(Vec3Cross)(float out[3], const float a[3], const float b[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

void FUNC_PROTO(Vec3Rotate)(float out[3], float v[3], float q[4])
{
	float cq[4];
	float p[4] = { v[0], v[1], v[2], 0 };

	FUNC_PROTO(QuaternionConjugate)(cq, q);

	FUNC_PROTO(QuaternionMultiply)(p, p, cq);
	FUNC_PROTO(QuaternionMultiply)(p, q, p);

	out[0] = p[0];
	out[1] = p[1];
	out[2] = p[2];
}

void FUNC_PROTO(Vec3Transform)(float out[3], const float v[3], const float m[16])
{
	float tmp[3];

	tmp[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8];
	tmp[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9];
	tmp[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10];

	out[0] = tmp[0];
	out[1] = tmp[1];
	out[2] = tmp[2];
}

void FUNC_PROTO(Vec3TransformTranspose)(float out[3], const float m[16], const float v[3])
{
	float tmp[3];

	tmp[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2];
	tmp[1] = v[0] * m[4] + v[1] * m[5] + v[2] * m[6];
	tmp[2] = v[0] * m[8] + v[1] * m[9] + v[2] * m[10];

	out[0] = tmp[0];
	out[1] = tmp[1];
	out[2] = tmp[2];
}

void FUNC_PROTO(Vec3TransformCoord)(float out[3], const float v[3], const float m[16])
{
	float tmp[4];

	tmp[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12];
	tmp[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13];
	tmp[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
	tmp[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + m[15];

	out[0] = tmp[0] / tmp[3];
	out[1] = tmp[1] / tmp[3];
	out[2] = tmp[2] / tmp[3];
}

void FUNC_PROTO(Vec3TransformCoordTranspose)(float out[3], const float m[16], const float v[3])
{
	float tmp[4];

	tmp[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2] + m[3];
	tmp[1] = v[0] * m[4] + v[1] * m[5] + v[2] * m[6] + m[7];
	tmp[2] = v[0] * m[8] + v[1] * m[9] + v[2] * m[10] + m[11];
	tmp[3] = v[0] * m[12] + v[1] * m[13] + v[2] * m[14] + m[15];

	out[0] = tmp[0] / tmp[3];
	out[1] = tmp[1] / tmp[3];
	out[2] = tmp[2] / tmp[3];
}

void FUNC_PROTO(Vec4Assign)(float out[4], const float a[4])
{
	out[0] = a[0];
	out[1] = a[1];
	out[2] = a[2];
	out[3] = a[3];
}

void FUNC_PROTO(Vec4Lerp)(float out[4], const float a[4], const float b[4], float s)
{
	float invs = 1.0f - s;
	
	out[0] = a[0] * invs + b[0] * s;
	out[1] = a[1] * invs + b[1] * s;
	out[2] = a[2] * invs + b[2] * s;
	out[3] = a[3] * invs + b[3] * s;
}

void FUNC_PROTO(Vec4Add)(float out[4], const float a[4], const float b[4])
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
	out[3] = a[3] + b[3];
}

void FUNC_PROTO(Vec4Subtract)(float out[4], const float a[4], const float b[4])
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
	out[3] = a[3] - b[3];
}

void FUNC_PROTO(Vec4Set)(float out[4], float x, float y, float z, float w)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

void FUNC_PROTO(Vec4Scale)(float out[4], const float a[4], float scale)
{
	out[0] = a[0] * scale;
	out[1] = a[1] * scale;
	out[2] = a[2] * scale;
	out[3] = a[3] * scale;
}

void FUNC_PROTO(Vec4Transform)(float out[4], const float v[4], const float m[16])
{
	float tmp[4];

	tmp[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
	tmp[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
	tmp[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
	tmp[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];

	out[0] = tmp[0];
	out[1] = tmp[1];
	out[2] = tmp[2];
	out[3] = tmp[3];
}

void FUNC_PROTO(Vec4TransformTranspose)(float out[4], const float m[16], const float v[4])
{
	float tmp[4];

	tmp[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2] + v[3] * m[3];
	tmp[1] = v[0] * m[4] + v[1] * m[5] + v[2] * m[6] + v[3] * m[7];
	tmp[2] = v[0] * m[8] + v[1] * m[9] + v[2] * m[10] + v[3] * m[11];
	tmp[3] = v[0] * m[12] + v[1] * m[13] + v[2] * m[14] + v[3] * m[15];

	out[0] = tmp[0];
	out[1] = tmp[1];
	out[2] = tmp[2];
	out[3] = tmp[3];
}

void FUNC_PROTO(PlaneFromRay)(float out[4], const float start[3], const float dir[3])
{
	out[0] = dir[0];
	out[1] = dir[1];
	out[2] = dir[2];
	out[3] = -FUNC_PROTO(Vec3Dot)(start, dir);
}

void FUNC_PROTO(PlaneNormalize)(float out[4], const float p[4])
{
	float il = 1.0f / FUNC_PROTO(Vec3Length)(p);

	out[0] = p[0] * il;
	out[1] = p[1] * il;
	out[2] = p[2] * il;
	out[3] = p[3] * il;
}

void FUNC_PROTO(MatrixAssign)(float out[16], const float m[16])
{
	out[0] = m[0];
	out[1] = m[1];
	out[2] = m[2];
	out[3] = m[3];
	
	out[4] = m[4];
	out[5] = m[5];
	out[6] = m[6];
	out[7] = m[7];
	
	out[8] = m[8];
	out[9] = m[9];
	out[10] = m[10];
	out[11] = m[11];
	
	out[12] = m[12];
	out[13] = m[13];
	out[14] = m[14];
	out[15] = m[15];
}

void FUNC_PROTO(MatrixSet)(float out[16], float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	out[0] = _11;
	out[1] = _12;
	out[2] = _13;
	out[3] = _14;
	
	out[4] = _21;
	out[5] = _22;
	out[6] = _23;
	out[7] = _24;
	
	out[8] = _31;
	out[9] = _32;
	out[10] = _33;
	out[11] = _34;
	
	out[12] = _41;
	out[13] = _42;
	out[14] = _43;
	out[15] = _44;
}

void FUNC_PROTO(MatrixViewVector)(float out[16], const float viewdir[3])
{
	float x[3];
	float y[3] = { 0, 1, 0 };
	float z[3];

	FUNC_PROTO(Vec3Normalize)(z, viewdir);

	float test = FUNC_PROTO(Vec3Dot)(y, z);

	if( fabs(test) > 0.98f )
	{
		y[0] = 1;
		y[1] = 0;
	}

	FUNC_PROTO(Vec3Cross)(x, y, z);
	FUNC_PROTO(Vec3Cross)(y, z, x);

	out[0] = x[0];	out[1] = y[0];	out[2] = z[0];
	out[4] = x[1];	out[5] = y[1];	out[6] = z[1];
	out[8] = x[2];	out[9] = y[2];	out[10] = z[2];

	out[3] = out[7] = out[11] = out[12] = out[13] = out[14] = 0;
	out[15] = 1;
}

void FUNC_PROTO(MatrixLookAtLH)(float out[16], const float eye[3], const float look[3], const float up[3])
{
	float x[3], y[3], z[3];

	z[0] = look[0] - eye[0];
	z[1] = look[1] - eye[1];
	z[2] = look[2] - eye[2];

	FUNC_PROTO(Vec3Normalize)(z, z);
	FUNC_PROTO(Vec3Cross)(x, up, z);

	FUNC_PROTO(Vec3Normalize)(x, x);
	FUNC_PROTO(Vec3Cross)(y, z, x);

	out[0] = x[0];		out[1] = y[0];		out[2] = z[0];		out[3] = 0.0f;
	out[4] = x[1];		out[5] = y[1];		out[6] = z[1];		out[7] = 0.0f;
	out[8] = x[2];		out[9] = y[2];		out[10] = z[2];		out[11] = 0.0f;

	out[12] = -FUNC_PROTO(Vec3Dot)(x, eye);
	out[13] = -FUNC_PROTO(Vec3Dot)(y, eye);
	out[14] = -FUNC_PROTO(Vec3Dot)(z, eye);
	out[15] = 1.0f;
}

void FUNC_PROTO(MatrixLookAtRH)(float out[16], const float eye[3], const float look[3], const float up[3])
{
	float x[3], y[3], z[3];

	z[0] = eye[0] - look[0];
	z[1] = eye[1] - look[1];
	z[2] = eye[2] - look[2];

	FUNC_PROTO(Vec3Normalize)(z, z);
	FUNC_PROTO(Vec3Cross)(x, up, z);

	FUNC_PROTO(Vec3Normalize)(x, x);
	FUNC_PROTO(Vec3Cross)(y, z, x);

	out[0] = x[0];		out[1] = y[0];		out[2] = z[0];		out[3] = 0.0f;
	out[4] = x[1];		out[5] = y[1];		out[6] = z[1];		out[7] = 0.0f;
	out[8] = x[2];		out[9] = y[2];		out[10] = z[2];		out[11] = 0.0f;

	out[12] = -FUNC_PROTO(Vec3Dot)(x, eye);
	out[13] = -FUNC_PROTO(Vec3Dot)(y, eye);
	out[14] = -FUNC_PROTO(Vec3Dot)(z, eye);
	out[15] = 1.0f;
}

void FUNC_PROTO(MatrixPerspectiveFovLH)(float out[16], float fovy, float aspect, float nearplane, float farplane)
{
#ifdef USE_VULKAN_PREFIX
	out[5] = -1.0f / tanf(fovy / 2);
	out[0] = out[5] / -aspect;
#else
	out[5] = 1.0f / tanf(fovy / 2);
	out[0] = out[5] / aspect;
#endif

	out[1] = out[2] = out[3] = 0;
	out[4] = out[6] = out[7] = 0;
	out[8] = out[9] = 0;
	out[12] = out[13] = out[15] = 0;

	out[11] = 1.0f;

#if defined(USE_VULKAN_PREFIX) || defined(USE_METAL_PREFIX) || defined(USE_D3D_PREFIX)
	// [0, 1]
	out[10] = farplane / (farplane - nearplane);
	out[14] = -nearplane * out[10];
#else
	// [-1, 1]
	out[10] = (farplane + nearplane) / (farplane - nearplane);
	out[14] = -2 * farplane * nearplane / (farplane - nearplane);
#endif
}

void FUNC_PROTO(MatrixPerspectiveFovRH)(float out[16], float fovy, float aspect, float nearplane, float farplane)
{
#ifdef USE_VULKAN_PREFIX
	out[5] = -1.0f / tanf(fovy / 2);
	out[0] = out[5] / -aspect;
#else
	out[5] = 1.0f / tanf(fovy / 2);
	out[0] = out[5] / aspect;
#endif

	out[1] = out[2] = out[3] = 0;
	out[4] = out[6] = out[7] = 0;
	out[8] = out[9] = 0;
	out[12] = out[13] = out[15] = 0;

	out[11] = -1.0f;

#if defined(USE_VULKAN_PREFIX) || defined(USE_METAL_PREFIX) || defined(USE_D3D_PREFIX)
	// [0, 1]
	out[10] = farplane / (nearplane - farplane);
	out[14] = nearplane * out[10];
#else
	// [-1, 1]
	out[10] = (farplane + nearplane) / (nearplane - farplane);
	out[14] = 2 * farplane * nearplane / (nearplane - farplane);
#endif
}

void FUNC_PROTO(MatrixOrthoRH)(float out[16], float left, float right, float bottom, float top, float nearplane, float farplane)
{
	out[1] = out[2] = 0;
	out[0] = 2.0f / (right - left);
	out[12] = -(right + left) / (right - left);

	out[4] = out[6] = 0;
	out[5] = 2.0f / (top - bottom);
	out[13] = -(top + bottom) / (top - bottom);

	out[8] = out[9] = 0;
	
#ifdef USE_VULKAN_PREFIX
	// [0, 1]
	out[10] =  1.0f / (nearplane - farplane);
	out[14] = nearplane / (nearplane - farplane);
#else
	// [-1, 1]
	out[10] = -2.0f / (farplane - nearplane);
	out[14] = -(farplane + nearplane) / (farplane - nearplane);
#endif

	out[3] = out[7] = out[11] = 0;
	out[15] = 1;
}

void FUNC_PROTO(MatrixMultiply)(float out[16], const float a[16], const float b[16])
{
	float tmp[16];

	tmp[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
	tmp[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
	tmp[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
	tmp[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];

	tmp[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
	tmp[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
	tmp[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
	tmp[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];

	tmp[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
	tmp[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
	tmp[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
	tmp[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];

	tmp[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
	tmp[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
	tmp[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
	tmp[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];

	memcpy(out, tmp, 16 * sizeof(float));
}

void FUNC_PROTO(MatrixTranslation)(float out[16], float x, float y, float z)
{
	FUNC_PROTO(MatrixIdentity)(out);

	out[12] = x;
	out[13] = y;
	out[14] = z;
}

void FUNC_PROTO(MatrixTranspose)(float out[16], float m[16])
{
	out[0] = m[0];
	out[1] = m[4];
	out[2] = m[8];
	out[3] = m[12];
	
	out[4] = m[1];
	out[5] = m[5];
	out[6] = m[9];
	out[7] = m[13];
	
	out[8] = m[2];
	out[9] = m[6];
	out[10] = m[10];
	out[11] = m[14];
	
	out[12] = m[3];
	out[13] = m[7];
	out[14] = m[11];
	out[15] = m[15];
}

void FUNC_PROTO(MatrixScaling)(float out[16], float x, float y, float z)
{
	FUNC_PROTO(MatrixIdentity)(out);

	out[0] = x;
	out[5] = y;
	out[10] = z;
}

void FUNC_PROTO(MatrixRotationAxis)(float out[16], float angle, float x, float y, float z)
{
	float u[3] = { x, y, z };

	float cosa = cosf(angle);
	float sina = sinf(angle);

	FUNC_PROTO(Vec3Normalize)(u, u);

	out[0] = cosa + u[0] * u[0] * (1.0f - cosa);
	out[4] = u[0] * u[1] * (1.0f - cosa) - u[2] * sina;
	out[8] = u[0] * u[2] * (1.0f - cosa) + u[1] * sina;
	out[12] = 0;

	out[1] = u[1] * u[0] * (1.0f - cosa) + u[2] * sina;
	out[5] = cosa + u[1] * u[1] * (1.0f - cosa);
	out[9] = u[1] * u[2] * (1.0f - cosa) - u[0] * sina;
	out[13] = 0;

	out[2] = u[2] * u[0] * (1.0f - cosa) - u[1] * sina;
	out[6] = u[2] * u[1] * (1.0f - cosa) + u[0] * sina;
	out[10] = cosa + u[2] * u[2] * (1.0f - cosa);
	out[14] = 0;

	out[3] = out[7] = out[11] = 0;
	out[15] = 1;
}

void FUNC_PROTO(MatrixRotationQuaternion)(float out[16], const float q[4])
{
	// TODO:
	out[0] = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
	out[1] = 2.0f * (q[0] * q[1] + q[2] * q[3]);
	out[2] = 2.0f * (q[0] * q[2] - q[1] * q[3]);

	out[4] = 2.0f * (q[0] * q[1] - q[2] * q[3]);
	out[5] = 1.0f - 2.0f * (q[0] * q[0] + q[2] * q[2]);
	out[6] = 2.0f * (q[1] * q[2] + q[0] * q[3]);

	out[8] = 2.0f * (q[0] * q[2] + q[1] * q[3]);
	out[9] = 2.0f * (q[1] * q[2] - q[0] * q[3]);
	out[10] = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);

	out[3] = out[7] = out[11] = 0;
	out[12] = out[13] = out[14] = 0;
	out[15] = 1;
}

void FUNC_PROTO(MatrixIdentity)(float out[16])
{
	memset(out, 0, 16 * sizeof(float));
	out[0] = out[5] = out[10] = out[15] = 1;
}

void FUNC_PROTO(MatrixInverse)(float out[16], const float m[16])
{
	float s[6] =
	{
		m[0] * m[5] - m[1] * m[4],
		m[0] * m[6] - m[2] * m[4],
		m[0] * m[7] - m[3] * m[4],
		m[1] * m[6] - m[2] * m[5],
		m[1] * m[7] - m[3] * m[5],
		m[2] * m[7] - m[3] * m[6]
	};

	float c[6] =
	{
		m[8] * m[13] - m[9] * m[12],
		m[8] * m[14] - m[10] * m[12],
		m[8] * m[15] - m[11] * m[12],
		m[9] * m[14] - m[10] * m[13],
		m[9] * m[15] - m[11] * m[13],
		m[10] * m[15] - m[11] * m[14]
	};

	float det = (s[0] * c[5] - s[1] * c[4] + s[2] * c[3] + s[3] * c[2] - s[4] * c[1] + s[5] * c[0]);

#ifdef _DEBUG
	if( fabs(det) < 1e-9f )
		throw 1;
#endif

	float r = 1.0f / det;

	out[0] = r * (m[5] * c[5] - m[6] * c[4] + m[7] * c[3]);
	out[1] = r * (m[2] * c[4] - m[1] * c[5] - m[3] * c[3]);
	out[2] = r * (m[13] * s[5] - m[14] * s[4] + m[15] * s[3]);
	out[3] = r * (m[10] * s[4] - m[9] * s[5] - m[11] * s[3]);

	out[4] = r * (m[6] * c[2] - m[4] * c[5] - m[7] * c[1]);
	out[5] = r * (m[0] * c[5] - m[2] * c[2] + m[3] * c[1]);
	out[6] = r * (m[14] * s[2] - m[12] * s[5] - m[15] * s[1]);
	out[7] = r * (m[8] * s[5] - m[10] * s[2] + m[11] * s[1]);

	out[8] = r * (m[4] * c[4] - m[5] * c[2] + m[7] * c[0]);
	out[9] = r * (m[1] * c[2] - m[0] * c[4] - m[3] * c[0]);
	out[10] = r * (m[12] * s[4] - m[13] * s[2] + m[15] * s[0]);
	out[11] = r * (m[9] * s[2] - m[8] * s[4] - m[11] * s[0]);

	out[12] = r * (m[5] * c[1] - m[4] * c[3] - m[6] * c[0]);
	out[13] = r * (m[0] * c[3] - m[1] * c[1] + m[2] * c[0]);
	out[14] = r * (m[13] * s[1] - m[12] * s[3] - m[14] * s[0]);
	out[15] = r * (m[8] * s[3] - m[9] * s[1] + m[10] * s[0]);
}

void FUNC_PROTO(MatrixReflect)(float out[16], float plane[4])
{
	float p[4];

	FUNC_PROTO(PlaneNormalize)(p, plane);

	out[0] = -2 * p[0] * p[0] + 1;
	out[1] = -2 * p[1] * p[0];
	out[2] = -2 * p[2] * p[0];

	out[4] = -2 * p[0] * p[1];
	out[5] = -2 * p[1] * p[1] + 1;
	out[6] = -2 * p[2] * p[1];

	out[8] = -2 * p[0] * p[2];
	out[9] = -2 * p[1] * p[2];
	out[10] = -2 * p[2] * p[2] + 1;

	out[12] = -2 * p[0] * p[3];
	out[13] = -2 * p[1] * p[3];
	out[14] = -2 * p[2] * p[3];

	out[3] = out[7] = out[11] = 0;
	out[15] = 1;
}

void FUNC_PROTO(QuaternionConjugate)(float out[4], float q[4])
{
	out[0] = -q[0];
	out[1] = -q[1];
	out[2] = -q[2];
	out[3] = q[3];
}

void FUNC_PROTO(QuaternionIdentity)(float out[4])
{
	out[0] = out[1] = out[2] = 0;
	out[3] = 1;
}

void FUNC_PROTO(QuaternionMultiply)(float out[4], float a[4], float b[4])
{
	float tmp[4];

	tmp[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	tmp[1] = a[3] * b[1] + a[1] * b[3] + a[2] * b[0] - a[0] * b[2];
	tmp[2] = a[3] * b[2] + a[2] * b[3] + a[0] * b[1] - a[1] * b[0];
	tmp[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];

	out[0] = tmp[0];
	out[1] = tmp[1];
	out[2] = tmp[2];
	out[3] = tmp[3];
}

void FUNC_PROTO(QuaternionSet)(float out[4], float x, float y, float z, float w)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

void FUNC_PROTO(QuaternionNormalize)(float out[4], float q[4])
{
	float il = 1.0f / sqrtf(q[3] * q[3] + q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);

	out[3] = q[3] * il;
	out[0] = q[0] * il;
	out[1] = q[1] * il;
	out[2] = q[2] * il;
}

void FUNC_PROTO(QuaternionRotationAxis)(float out[4], float x, float y, float z, float angle)
{
	float l = sqrtf(x * x + y * y + z * z);
	float ha = angle * 0.5f;
	float sa = sinf(ha);

	out[0] = (x / l) * sa;
	out[1] = (y / l) * sa;
	out[2] = (z / l) * sa;
	out[3] = cosf(ha);
}

void FUNC_PROTO(FrustumPlanes)(float out[6][4], const float viewproj[16])
{
	FUNC_PROTO(Vec4Set)(out[0], viewproj[0] + viewproj[3], viewproj[4] + viewproj[7], viewproj[8] + viewproj[11], viewproj[12] + viewproj[15]);		// left
	FUNC_PROTO(Vec4Set)(out[1], viewproj[3] - viewproj[0], viewproj[7] - viewproj[4], viewproj[11] - viewproj[8], viewproj[15] - viewproj[12]);		// right
	FUNC_PROTO(Vec4Set)(out[2], viewproj[3] - viewproj[1], viewproj[7] - viewproj[5], viewproj[11] - viewproj[9], viewproj[15] - viewproj[13]);		// top
	FUNC_PROTO(Vec4Set)(out[3], viewproj[1] + viewproj[3], viewproj[5] + viewproj[7], viewproj[9] + viewproj[11], viewproj[13] + viewproj[15]);		// bottom
	FUNC_PROTO(Vec4Set)(out[4], viewproj[2], viewproj[6], viewproj[10], viewproj[14]);																// near
	FUNC_PROTO(Vec4Set)(out[5], viewproj[3] - viewproj[2], viewproj[7] - viewproj[6], viewproj[11] - viewproj[10], viewproj[15] - viewproj[14]);	// far

	FUNC_PROTO(PlaneNormalize)(out[0], out[0]);
	FUNC_PROTO(PlaneNormalize)(out[1], out[1]);
	FUNC_PROTO(PlaneNormalize)(out[2], out[2]);
	FUNC_PROTO(PlaneNormalize)(out[3], out[3]);
	FUNC_PROTO(PlaneNormalize)(out[4], out[4]);
	FUNC_PROTO(PlaneNormalize)(out[5], out[5]);
}

void FUNC_PROTO(FitToBox)(float& outnear, float& outfar, const float eye[3], const float look[3], const CLASS_PROTO(AABox)& box)
{
	float refplane[4];
	float length;

	refplane[0] = look[0] - eye[0];
	refplane[1] = look[1] - eye[1];
	refplane[2] = look[2] - eye[2];
	refplane[3] = -(refplane[0] * eye[0] + refplane[1] * eye[1] + refplane[2] * eye[2]);

	length = FUNC_PROTO(Vec3Length)(refplane);

	refplane[0] /= length;
	refplane[1] /= length;
	refplane[2] /= length;
	refplane[3] /= length;

	outnear = box.Nearest(refplane) - 0.02f;	// 2mm
	outfar = box.Farthest(refplane) + 0.02f;	// 2mm

	outnear = FUNC_PROTO(Max)<float>(outnear, 0.1f);
	outfar = FUNC_PROTO(Max)<float>(outfar, 0.2f);
}

void FUNC_PROTO(FitToBox)(float out[16], float outclip[2], const float view[16], const CLASS_PROTO(AABox)& box)
{
	float pleft[] = { 1, 0, 0, 0 };
	float pbottom[] = { 0, 1, 0, 0 };
	float pnear[] = { 0, 0, -1, 0 };

	FUNC_PROTO(Vec4TransformTranspose)(pleft, view, pleft);
	FUNC_PROTO(Vec4TransformTranspose)(pbottom, view, pbottom);
	FUNC_PROTO(Vec4TransformTranspose)(pnear, view, pnear);

	float left = box.Nearest(pleft) - pleft[3];
	float right = box.Farthest(pleft) - pleft[3];
	float bottom = box.Nearest(pbottom) - pbottom[3];
	float top = box.Farthest(pbottom) - pbottom[3];

	outclip[0] = box.Nearest(pnear) - pnear[3];
	outclip[1] = box.Farthest(pnear) - pnear[3];

	FUNC_PROTO(MatrixOrthoRH)(out, left, right, bottom, top, outclip[0], outclip[1]);
}

void FUNC_PROTO(GetOrthogonalVectors)(float out1[3], float out2[3], const float v[3])
{
	// select dominant direction
	int bestcoord = 0;

	if( fabs(v[1]) > fabs(v[bestcoord]) )
		bestcoord = 1;

	if( fabs(v[2]) > fabs(v[bestcoord]) )
		bestcoord = 2;

	// ignore handedness
	int other1 = (bestcoord + 1) % 3;
	int other2 = (bestcoord + 2) % 3;

	out1[bestcoord] = v[other1];
	out1[other1] = -v[bestcoord];
	out1[other2] = v[other2];

	out2[bestcoord] = v[other2];
	out2[other1] = v[other1];
	out2[other2] = -v[bestcoord];
}

float FUNC_PROTO(RayIntersectSphere)(const float c[3], const float radius, const float start[3], const float dir[3])
{
	float smc[3];
	float v1, v2, v3;
	float d, t1, t2;

	FUNC_PROTO(Vec3Subtract)(smc, start, c);

	v1 = FUNC_PROTO(Vec3Dot)(dir, dir);
	v2 = 2 * FUNC_PROTO(Vec3Dot)(dir, smc);
	v3 = FUNC_PROTO(Vec3Dot)(smc, smc) * radius * radius;

	d = v2 * v2 - 4 * v1 * v3;

	if( d < 0.0f )
		return FLT_MAX;

	d = sqrtf(d);

	t1 = -v2 + d / (2 * v1);
	t2 = -v2 - d / (2 * v1);

	return FUNC_PROTO(Min)(t1, t2);
}

float FUNC_PROTO(RayIntersectCylinder)(const float a[3], const float b[3], const float radius, const float start[3], const float dir[3])
{
	float bma[3];
	float sma[3];
	float q[3], r[3];
	float m, n, d;
	float v1, v2, v3;
	float t, u, t1, t2;

	FUNC_PROTO(Vec3Subtract)(bma, b, a);
	FUNC_PROTO(Vec3Subtract)(sma, start, a);

	d = FUNC_PROTO(Vec3Dot)(bma, bma);

	if( fabs(d) < 1e-5f )
		return FLT_MAX;

	m = FUNC_PROTO(Vec3Dot)(bma, dir) / d;
	n = FUNC_PROTO(Vec3Dot)(bma, sma) / d;

	FUNC_PROTO(Vec3Scale)(q, bma, m);
	FUNC_PROTO(Vec3Subtract)(q, dir, q);

	FUNC_PROTO(Vec3Scale)(r, bma, n);
	FUNC_PROTO(Vec3Subtract)(r, sma, r);

	v1 = FUNC_PROTO(Vec3Dot)(q, q);
	v2 = 2 * FUNC_PROTO(Vec3Dot)(q, r);
	v3 = FUNC_PROTO(Vec3Dot)(r, r) - radius * radius;

	d = v2 * v2 - 4 * v1 * v3;

	if( d < 0.0f )
		return FLT_MAX;

	d = sqrtf(d);

	t1 = -v2 + d / (2 * v1);
	t2 = -v2 - d / (2 * v1);

	// only the outside is interesting (for now)
	t = FUNC_PROTO(Min)(t1, t2);
	u = t * m + n;

	if( u < 0.0f || u > 1.0f )
		t = FLT_MAX;

	return t;
}

float FUNC_PROTO(RayIntersectCapsule)(const float a[3], const float b[3], const float radius, const float start[3], const float dir[3])
{
	float t = FUNC_PROTO(RayIntersectCylinder)(a, b, radius, start, dir);

	if( t == FLT_MAX )
	{
		t = FUNC_PROTO(Min)(t, FUNC_PROTO(RayIntersectSphere)(a, radius, start, dir));
		t = FUNC_PROTO(Min)(t, FUNC_PROTO(RayIntersectSphere)(b, radius, start, dir));
	}

	return t;
}

float FUNC_PROTO(HalfToFloat)(uint16_t bits)
{
	uint32_t magic = 126 << 23;
	uint32_t fp32 = (bits & 0x8000) << 16;
	uint32_t mant = (bits & 0x000003ff);
	int32_t exp = (bits >> 10) & 0x0000001f;

	if( exp == 0 )
	{
		fp32 = magic + mant;
		(*(float*)&fp32) -= (*(float*)&magic);
	}
	else
	{
		mant <<= 13;

		if( exp == 31 )
			exp = 255;
		else
			exp += 127 - 15;

		fp32 |= (exp << 23);
		fp32 |= mant;
	}

	return *((float*)&fp32);
}

int FUNC_PROTO(FrustumIntersect)(float frustum[6][4], const CLASS_PROTO(AABox)& box)
{
	float center[3];
	float halfsize[3];
	float dist, maxdist;
	int result = 2; // inside

	box.GetCenter(center);
	box.GetHalfSize(halfsize);

	for( int j = 0; j < 6; ++j )
	{
		float* plane = frustum[j];

		dist = FUNC_PROTO(PlaneDistance)(plane, center);
		maxdist = fabs(plane[0] * halfsize[0]) + fabs(plane[1] * halfsize[1]) + fabs(plane[2] * halfsize[2]);

		if( dist < -maxdist )
			return 0;	// outside
		else if( fabs(dist) < maxdist )
			result = 1;	// intersect
	}

	return result;
}

uint32_t FUNC_PROTO(Vec3ToUbyte4)(const float a[3])
{
	uint32_t ret = 0;
	uint8_t* bytes = (uint8_t*)(&ret);
	
	bytes[0] = FUNC_PROTO(FloatToByte)((a[0] + 1.0f) * (255.0f / 2.0f) + 0.5f);
	bytes[1] = FUNC_PROTO(FloatToByte)((a[1] + 1.0f) * (255.0f / 2.0f) + 0.5f);
	bytes[2] = FUNC_PROTO(FloatToByte)((a[2] + 1.0f) * (255.0f / 2.0f) + 0.5f);
	
	return ret;
}

uint16_t FUNC_PROTO(FloatToHalf)(float f)
{
	uint32_t u = *(uint32_t*)(&f);
	uint32_t signbit = (u & 0x80000000) >> 16;
	int32_t exponent = ((u & 0x7F800000) >> 23) - 112;
	uint32_t mantissa = (u & 0x007FFFFF);
	
	if (exponent <= 0)
		return 0;
	
	if (exponent > 30)
		return (uint16_t)(signbit | 0x7BFF);
	
	return (uint16_t)(signbit | (exponent << 10) | (mantissa >> 13));
}

uint8_t FUNC_PROTO(FloatToByte)(float f)
{
	int32_t i = (int32_t)f;
	
	if (i < 0)
		return 0;
	else if (i > 255)
		return 255;
	
	return (uint8_t)i;
}

std::string& FUNC_PROTO(GetPath)(std::string& out, const std::string& str)
{
	size_t pos = str.find_last_of("\\/");

	if( pos != std::string::npos )
#ifdef _WIN32
		out = str.substr(0, pos) + '\\';
#else
		out = str.substr(0, pos) + '/';
#endif
	else
		out = "";

	return out;
}

std::string& FUNC_PROTO(GetFile)(std::string& out, const std::string& str)
{
	size_t pos = str.find_last_of("\\/");

	if( pos != std::string::npos )
		out = str.substr(pos + 1);
	else
		out = str;

	return out;
}

std::string& FUNC_PROTO(GetExtension)(std::string& out, const std::string& str)
{
	size_t pos = str.find_last_of(".");

	if( pos == std::string::npos ) {
		out = "";
		return out;
	}

	out = str.substr(pos + 1, str.length());
	return FUNC_PROTO(ToLower)(out, out);
}

std::string& FUNC_PROTO(ToLower)(std::string& out, const std::string& str)
{
	out.resize(str.size());

	for( size_t i = 0; i < str.size(); ++i ) {
		if( str[i] >= 'A' && str[i] <= 'Z' )
			out[i] = str[i] + 32;
		else
			out[i] = str[i];
	}

	return out;
}
