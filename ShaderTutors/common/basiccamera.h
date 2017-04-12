
#ifndef _BASICCAMERA_H_
#define _BASICCAMERA_H_

#include "3Dmath.h"

class BasicCamera
{
private:
	array_state<float, 3>	anglecurve;
	float					targetangles[3];
	float					smoothedangles[3];

	float					position[3];	// orbit point
	float					distance;		// distance from lookat
	float					nearplane;
	float					farplane;
	float					fov;
	float					aspect;
	bool					finished;
	
public:
	BasicCamera();
	
	void OrbitRight(float angle);
	void OrbitUp(float angle);
	
	void GetViewMatrix(float out[16]) const;
	void GetProjectionMatrix(float out[16]) const;
	void GetPosition(float out[3]) const;
	void GetEyePosition(float out[3]) const;
	
	void Update(float dt);
	void Animate(float alpha);

	inline void SetAspect(float value)					{ aspect = value; }
	inline void SetFov(float value)						{ fov = value; }
	inline void SetClipPlanes(float pnear, float pfar)	{ nearplane = pnear; farplane = pfar; }
	inline void SetDistance(float value)				{ distance = value; }
	inline void SetPosition(float x, float y, float z)	{ FUNC_PROTO(Vec3Set)(position, x, y, z); }
	
	inline float GetNearPlane() const					{ return nearplane; }
	inline float GetFarPlane() const					{ return farplane; }
	inline float GetFov() const							{ return fov; }

	inline bool IsAnimationFinished() const				{ return finished; }
};

#endif
