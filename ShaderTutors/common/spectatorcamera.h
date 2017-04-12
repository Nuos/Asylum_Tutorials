
#ifndef _SPECTATORCAMERA_H_
#define _SPECTATORCAMERA_H_

#include "3Dmath.h"

class SpectatorCamera
{
	enum {
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
	array_state<float, 3>	positioncurve;
	float					targetangles[3];
	float					smoothedangles[3];
	float					smoothedposition[3];
	uint32_t				state;
	bool					finished;

	void GetViewVectors(float forward[3], float right[3], float up[3]);

public:
	float Aspect;
	float Fov;
	float Near;
	float Far;

	SpectatorCamera();

	void FitToBox(const CLASS_PROTO(AABox)& box);

	void GetEyePosition(float out[3]);
	void GetViewMatrix(float out[16]);
	void GetProjectionMatrix(float out[16]);

	void SetEyePosition(float x, float y, float z);
	void SetOrientation(float yaw, float pitch, float roll);

	void Update(float dt);
	void Animate(float alpha);

	void Event_KeyDown(uint8_t keycode);
	void Event_KeyUp(uint8_t keycode);
	void Event_MouseMove(int16_t dx, int16_t dy);
	void Event_MouseDown(uint8_t button);
	void Event_MouseUp(uint8_t button);

	inline bool IsAnimationFinished() const {
		return finished;
	}
};

#endif
