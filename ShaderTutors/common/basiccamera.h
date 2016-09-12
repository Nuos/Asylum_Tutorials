
#ifndef _BASICCAMERA_H_
#define _BASICCAMERA_H_

#include "3Dmath.h"

class BasicCamera
{
private:
	float position[3];	// orbit point
	float angles[3];
	float distance;		// distance from lookat
	float nearplane;
	float farplane;
	float fov;
	float aspect;
	
public:
	BasicCamera();
	
	void OrbitRight(float angle);
	void OrbitUp (float angle);
	
	void GetViewMatrix (float out[16]) const;
	void GetProjectionMatrix (float out[16]) const;
	void GetEyePosition (float out[3]) const;
	
	inline void SetAspect(float value)					{ aspect = value; }
	inline void SetFov(float value)						{ fov = value; }
	inline void SetClipPlanes(float pnear, float pfar)	{ nearplane = pnear; farplane = pfar; }
	inline void SetDistance(float value)				{ distance = value; }
	inline void SetPosition(float x, float y, float z)	{ FUNC_PROTO(Vec3Set)(position, x, y, z); }
	
	inline float GetNearPlane() const					{ return nearplane; }
	inline float GetFarPlane() const					{ return farplane; }
};

#endif
