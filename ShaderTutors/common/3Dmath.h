
#ifndef _3DMATH_H_
#define _3DMATH_H_

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <string>

#if defined(USE_VULKAN_PREFIX)
#	define FUNC_PROTO(x)	VK##x
#	define CLASS_PROTO(x)	Vulkan##x
#elif defined(USE_METAL_PREFIX)
#	define FUNC_PROTO(x)	MTL##x
#	define CLASS_PROTO(x)	Metal##x
#elif defined(USE_D3D_PREFIX)
#	define FUNC_PROTO(x)	D3D##x
#	define CLASS_PROTO(x)	Direct3D##x
#else
#	define USE_OPENGL_PREFIX

#	define FUNC_PROTO(x)	GL##x
#	define CLASS_PROTO(x)	OpenGL##x
#endif

// what: GL{[a-zA-Z0-9]+}\(
// with: FUNC_PROTO(\1)(

static const float FUNC_PROTO(_PI) = 3.141592f;
static const float FUNC_PROTO(_2PI) = 6.283185f;
static const float FUNC_PROTO(_HALF_PI) = 1.570796f;

class CLASS_PROTO(Color)
{
public:
	float r, g, b, a;

	CLASS_PROTO(Color)();
	CLASS_PROTO(Color)(float _r, float _g, float _b, float _a);
	CLASS_PROTO(Color)(uint32_t argb32);

	static CLASS_PROTO(Color) Lerp(const CLASS_PROTO(Color)& from, const CLASS_PROTO(Color)& to, float frac);
	static CLASS_PROTO(Color) sRGBToLinear(uint8_t red, uint8_t green, uint8_t blue);

	static inline uint8_t ArgbA32(uint32_t c) {
		return ((uint8_t)((c >> 24) & 0xff));
	}

	static inline uint8_t ArgbR32(uint32_t c) {
		return ((uint8_t)((c >> 16) & 0xff));
	}

	static inline uint8_t ArgbG32(uint32_t c) {
		return ((uint8_t)((c >> 8) & 0xff));
	}

	static inline uint8_t ArgbB32(uint32_t c) {
		return ((uint8_t)(c & 0xff));
	}
};

/**
 * \brief Axis-aligned bounding box
 */
class CLASS_PROTO(AABox)
{
public:
	float Min[3];
	float Max[3];

	CLASS_PROTO(AABox)();
	CLASS_PROTO(AABox)(const CLASS_PROTO(AABox)& other);
	CLASS_PROTO(AABox)(const float size[3]);
	CLASS_PROTO(AABox)(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax);

	CLASS_PROTO(AABox)& operator =(const CLASS_PROTO(AABox)& other);

	bool Intersects(const CLASS_PROTO(AABox)& other) const;

	void Add(float x, float y, float z);
	void Add(const float v[3]);
	void GetCenter(float out[3]) const;
	void GetSize(float out[3]) const;
	void GetHalfSize(float out[3]) const;
	void GetPlanes(float outplanes[6][4]) const;
	void Inset(float dx, float dy, float dz);
	void TransformAxisAligned(const float traf[16]);

	float Radius() const;
	float RayIntersect(const float start[3], const float dir[3]) const;
	float Nearest(float from[4]) const;
	float Farthest(float from[4]) const;
};

int32_t FUNC_PROTO(ISqrt)(int32_t n);
uint32_t FUNC_PROTO(NextPow2)(uint32_t x);
uint32_t FUNC_PROTO(Log2OfPow2)(uint32_t x);

float FUNC_PROTO(Vec3Dot)(const float a[3], const float b[3]);
float FUNC_PROTO(Vec3Length)(const float a[3]);
float FUNC_PROTO(Vec3Distance)(const float a[3], const float b[3]);
float FUNC_PROTO(Vec4Dot)(const float a[4], const float b[4]);
float FUNC_PROTO(PlaneDistance)(const float p[4], const float v[3]);

void FUNC_PROTO(Vec3Assign)(float out[3], const float a[3]);
void FUNC_PROTO(Vec3Set)(float out[3], float x, float y, float z);
void FUNC_PROTO(Vec3Add)(float out[3], const float a[3], const float b[3]);
void FUNC_PROTO(Vec3Mad)(float out[3], const float a[3], const float b[3], float s);
void FUNC_PROTO(Vec3Subtract)(float out[3], const float a[3], const float b[3]);
void FUNC_PROTO(Vec3Scale)(float out[3], const float a[3], float scale);
void FUNC_PROTO(Vec3Modulate)(float out[3], const float a[3], const float b[3]);
void FUNC_PROTO(Vec3Swap)(float a[3], float b[3]);
void FUNC_PROTO(Vec3Normalize)(float out[3], const float v[3]);
void FUNC_PROTO(Vec3Cross)(float out[3], const float a[3], const float b[3]);
void FUNC_PROTO(Vec3Rotate)(float out[3], float v[3], float q[4]);
void FUNC_PROTO(Vec3Transform)(float out[3], const float v[3], const float m[16]);
void FUNC_PROTO(Vec3TransformTranspose)(float out[3], const float m[16], const float v[3]);
void FUNC_PROTO(Vec3TransformCoord)(float out[3], const float v[3], const float m[16]);

void FUNC_PROTO(Vec4Assign)(float out[4], const float a[4]);
void FUNC_PROTO(Vec4Lerp)(float out[4], const float a[4], const float b[4], float s);
void FUNC_PROTO(Vec4Set)(float out[4], float x, float y, float z, float w);
void FUNC_PROTO(Vec4Add)(float out[4], const float a[4], const float b[4]);
void FUNC_PROTO(Vec4Subtract)(float out[4], const float a[4], const float b[4]);
void FUNC_PROTO(Vec4Scale)(float out[4], const float a[4], float scale);
void FUNC_PROTO(Vec4Transform)(float out[4], const float v[4], const float m[16]);
void FUNC_PROTO(Vec4TransformTranspose)(float out[4], const float m[16], const float v[4]);

void FUNC_PROTO(PlaneFromRay)(float out[4], const float start[3], const float dir[3]);
void FUNC_PROTO(PlaneNormalize)(float out[4], const float p[4]);

void FUNC_PROTO(MatrixAssign)(float out[16], const float m[16]);
void FUNC_PROTO(MatrixSet)(float out[16], float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44);
void FUNC_PROTO(MatrixViewVector)(float out[16], const float viewdir[3]);
void FUNC_PROTO(MatrixLookAtLH)(float out[16], const float eye[3], const float look[3], const float up[3]);
void FUNC_PROTO(MatrixLookAtRH)(float out[16], const float eye[3], const float look[3], const float up[3]);
void FUNC_PROTO(MatrixPerspectiveFovLH)(float out[16], float fovy, float aspect, float nearplane, float farplane);
void FUNC_PROTO(MatrixPerspectiveFovRH)(float out[16], float fovy, float aspect, float nearplane, float farplane);
void FUNC_PROTO(MatrixOrthoRH)(float out[16], float left, float right, float bottom, float top, float nearplane, float farplane);
void FUNC_PROTO(MatrixMultiply)(float out[16], const float a[16], const float b[16]);
void FUNC_PROTO(MatrixTranslation)(float out[16], float x, float y, float z);
void FUNC_PROTO(MatrixTranspose)(float out[16], float m[16]);
void FUNC_PROTO(MatrixScaling)(float out[16], float x, float y, float z);
void FUNC_PROTO(MatrixRotationAxis)(float out[16], float angle, float x, float y, float z);
void FUNC_PROTO(MatrixRotationYawPitchRoll)(float out[16], float yaw, float pitch, float roll);
void FUNC_PROTO(MatrixRotationRollPitchYaw)(float out[16], float roll, float pitch, float yaw);
void FUNC_PROTO(MatrixRotationQuaternion)(float out[16], const float q[4]);
void FUNC_PROTO(MatrixIdentity)(float out[16]);
void FUNC_PROTO(MatrixInverse)(float out[16], const float m[16]);
void FUNC_PROTO(MatrixReflect)(float out[16], float plane[4]);

void FUNC_PROTO(QuaternionConjugate)(float out[4], float q[4]);
void FUNC_PROTO(QuaternionIdentity)(float out[4]);
void FUNC_PROTO(QuaternionMultiply)(float out[4], float a[4], float b[4]);
void FUNC_PROTO(QuaternionSet)(float out[4], float x, float y, float z, float w);
void FUNC_PROTO(QuaternionNormalize)(float out[4], float q[4]);
void FUNC_PROTO(QuaternionRotationAxis)(float out[4], float x, float y, float z, float angle);
void FUNC_PROTO(QuaternionRotationYawPitchRoll)(float out[4], float yaw, float pitch, float roll);
void FUNC_PROTO(QuaternionForward)(float out[3], const float q[4]);
void FUNC_PROTO(QuaternionRight)(float out[3], const float q[4]);
void FUNC_PROTO(QuaternionUp)(float out[3], const float q[4]);

void FUNC_PROTO(FrustumPlanes)(float out[6][4], const float viewproj[16]);
void FUNC_PROTO(FitToBox)(float& outnear, float& outfar, const float eye[3], const float look[3], const CLASS_PROTO(AABox)& box);
void FUNC_PROTO(FitToBox)(float out[16], float outclip[2], const float view[16], const CLASS_PROTO(AABox)& box);
void FUNC_PROTO(GetOrthogonalVectors)(float out1[3], float out2[3], const float v[3]);

float FUNC_PROTO(RayIntersectSphere)(const float c[3], const float radius, const float start[3], const float dir[3]);
float FUNC_PROTO(RayIntersectCylinder)(const float a[3], const float b[3], const float radius, const float start[3], const float dir[3]);
float FUNC_PROTO(RayIntersectCapsule)(const float a[3], const float b[3], const float radius, const float start[3], const float dir[3]);
float FUNC_PROTO(HalfToFloat)(uint16_t bits);

int FUNC_PROTO(FrustumIntersect)(float frustum[6][4], const CLASS_PROTO(AABox)& box);
uint32_t FUNC_PROTO(Vec3ToUbyte4)(const float a[3]);
uint16_t FUNC_PROTO(FloatToHalf)(float f);
uint8_t FUNC_PROTO(FloatToByte)(float f);

std::string& FUNC_PROTO(GetPath)(std::string& out, const std::string& str);
std::string& FUNC_PROTO(GetFile)(std::string& out, const std::string& str);
std::string& FUNC_PROTO(GetExtension)(std::string& out, const std::string& str);
std::string& FUNC_PROTO(ToLower)(std::string& out, const std::string& str);

// other
template <typename T, int n>
struct array_state
{
	T prev[n];
	T curr[n];

	array_state& operator =(T t[n]) {
		for (int i = 0; i < n; ++i)
			prev[i] = curr[i] = t[i];

		return *this;
	}

	void extend(T t[n]) {
		for (int i = 0; i < n; ++i) {
			prev[i] = curr[i];
			curr[i] += t[i];
		}
	}

	void smooth(T out[n], float alpha) {
		for (int i = 0; i < n; ++i)
			out[i] = prev[i] + alpha * (curr[i] - prev[i]);
	}
};

template <typename T, int n>
void array_state_set(array_state<T, n>& arr, float f1, float f2)
{
	arr.prev[0] = arr.curr[0] = f1;
	arr.prev[1] = arr.curr[1] = f2;
}

template <typename T, int n>
void array_state_set(array_state<T, n>& arr, float f1, float f2, float f3)
{
	arr.prev[0] = arr.curr[0] = f1;
	arr.prev[1] = arr.curr[1] = f2;
	arr.prev[2] = arr.curr[2] = f3;
}

template <typename T>
inline const T& FUNC_PROTO(Min)(const T& a, const T& b) {
	return ((a < b) ? a : b);
}

template <typename T>
inline const T& FUNC_PROTO(Max)(const T& a, const T& b) {
	return ((a > b) ? a : b);
}

inline float FUNC_PROTO(DegreesToRadians)(float value) {
	return value * (FUNC_PROTO(_PI) / 180.0f);
}

inline float FUNC_PROTO(RadiansToDegrees)(float value) {
	return value * (180.0f / FUNC_PROTO(_PI));
}

inline float FUNC_PROTO(Clamp)(float value, float min, float max) {
	return FUNC_PROTO(Min)(FUNC_PROTO(Max)(min, value), max);
}

inline float FUNC_PROTO(RandomFloat)() {
	return (rand() & 32767) / 32767.0f;
}

#endif
