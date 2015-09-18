
#ifndef _FPSCAMERA_H_
#define _FPSCAMERA_H_

#include "gl4x.h"

class CollisionWorld;
class RigidBody;

class FPSCamera
{
	enum CameraState
	{
		State_Rotating = 1,
		State_Forward = 2,
		State_Backward = 4,
		State_Left = 8,
		State_Right = 16,
		State_Moving = (State_Forward|State_Backward|State_Left|State_Right)
	};

private:
	float					view[16];
	array_state<float, 3>	anglecurve;
	float					position[3];
	float					targetangles[3];
	float					smoothedangles[3];
	CollisionWorld*			collworld;
	RigidBody*				body;
	unsigned int			state;
	bool					isonground;

	void GetViewVectors(float forward[3], float right[3], float up[3]);

public:
	float Aspect;
	float Fov;
	float Near;
	float Far;

	FPSCamera(CollisionWorld* world);

	void FitToBox(const OpenGLAABox& box);

	void GetEyePosition(float out[3]);
	void GetViewMatrix(float out[16]);
	void GetProjectionMatrix(float out[16]);

	void SetEyePosition(float x, float y, float z);
	void SetOrientation(float yaw, float pitch, float roll);

	void Update(float dt);
	void Animate(float alpha);

	void Event_KeyDown(unsigned char keycode);
	void Event_KeyUp(unsigned char keycode);
	void Event_MouseMove(short dx, short dy);
	void Event_MouseDown(unsigned char button);
	void Event_MouseUp(unsigned char button);
};

#endif
