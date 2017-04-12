
#include "basiccamera.h"

#define ROTATIONAL_SPEED		0.75f	// rad/s
#define ROTATIONAL_INVINTERTIA	5.0f

BasicCamera::BasicCamera()
{
	distance	= 1.0f;
	nearplane	= 0.1f;
	farplane	= 50.0f;
	fov			= FUNC_PROTO(_PI) / 3;
	aspect		= 4.0f / 3.0f;
	finished	= true;
	
	array_state_set(anglecurve, 0, 0, 0);

	FUNC_PROTO(Vec3Set)(position, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(targetangles, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(smoothedangles, 0, 0, 0);
}

void BasicCamera::OrbitRight(float angle)
{
	targetangles[0] -= angle * ROTATIONAL_SPEED;
	finished = false;
}

void BasicCamera::OrbitUp(float angle)
{
	targetangles[1] -= angle * ROTATIONAL_SPEED;
	targetangles[1] = FUNC_PROTO(Clamp)(targetangles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));

	finished = false;
}

void BasicCamera::GetViewMatrix(float out[16]) const
{
	float eye[3];
	
	GetEyePosition(eye);
	FUNC_PROTO(MatrixRotationYawPitchRoll)(out, smoothedangles[0], smoothedangles[1], smoothedangles[2]);
	
	out[12] = -(eye[0] * out[0] + eye[1] * out[4] + eye[2] * out[8]);
	out[13] = -(eye[0] * out[1] + eye[1] * out[5] + eye[2] * out[9]);
	out[14] = -(eye[0] * out[2] + eye[1] * out[6] + eye[2] * out[10]);
}

void BasicCamera::GetProjectionMatrix(float out[16]) const
{
	return FUNC_PROTO(MatrixPerspectiveFovRH)(out, fov, aspect, nearplane, farplane);
}

void BasicCamera::GetPosition (float out[3]) const
{
	FUNC_PROTO(Vec3Assign)(out, position);
}

void BasicCamera::GetEyePosition(float out[3]) const
{
	float q[4];
	float forward[3];
	
	FUNC_PROTO(QuaternionRotationYawPitchRoll)(q, smoothedangles[0], smoothedangles[1], smoothedangles[2]);
	FUNC_PROTO(QuaternionForward)(forward, q);
	
	// view space is right handed
	FUNC_PROTO(Vec3Scale)(forward, forward, -distance);
	FUNC_PROTO(Vec3Subtract)(out, position, forward);
}

void BasicCamera::Update(float dt)
{
	float diff[3];

	// rotate
	targetangles[1] = FUNC_PROTO(Clamp)(targetangles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));

	diff[0] = (targetangles[0] - anglecurve.curr[0]) * dt * ROTATIONAL_INVINTERTIA;
	diff[1] = (targetangles[1] - anglecurve.curr[1]) * dt * ROTATIONAL_INVINTERTIA;
	diff[2] = 0;

	if( FUNC_PROTO(Vec3Dot)(diff, diff) < 1e-4f )
		finished = true;

	anglecurve.extend(diff);
}

void BasicCamera::Animate(float alpha)
{
	anglecurve.smooth(smoothedangles, alpha);
}
