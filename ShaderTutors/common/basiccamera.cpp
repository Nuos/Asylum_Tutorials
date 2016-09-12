
#include "basiccamera.h"

BasicCamera::BasicCamera()
{
	distance	= 1.0f;
	nearplane	= 0.1f;
	farplane	= 50.0f;
	fov			= FUNC_PROTO(_PI) / 3;
	aspect		= 4.0f / 3.0f;
	
	FUNC_PROTO(Vec3Set)(position, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(angles, 0, 0, 0);
}

void BasicCamera::OrbitRight(float angle)
{
	angles[0] = fmodf (angles[0] - angle, FUNC_PROTO(_2PI));
}

void BasicCamera::OrbitUp(float angle)
{
	angles[1] = fmodf (angles[1] + angle, FUNC_PROTO(_2PI));
	angles[1] = FUNC_PROTO(Clamp)(angles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));
}

void BasicCamera::GetViewMatrix(float out[16]) const
{
	float eye[3];
	
	GetEyePosition(eye);
	FUNC_PROTO(MatrixRotationYawPitchRoll)(out, angles[0], angles[1], angles[2]);
	
	out[12] = -(eye[0] * out[0] + eye[1] * out[4] + eye[2] * out[8]);
	out[13] = -(eye[0] * out[1] + eye[1] * out[5] + eye[2] * out[9]);
	out[14] = -(eye[0] * out[2] + eye[1] * out[6] + eye[2] * out[10]);
}

void BasicCamera::GetProjectionMatrix(float out[16]) const
{
	return FUNC_PROTO(MatrixPerspectiveFovLH)(out, fov, aspect, nearplane, farplane);
}

void BasicCamera::GetEyePosition(float out[3]) const
{
	float q[4];
	float forward[3];
	
	FUNC_PROTO(QuaternionRotationYawPitchRoll)(q, angles[0], angles[1], angles[2]);
	FUNC_PROTO(QuaternionForward)(forward, q);
	
	FUNC_PROTO(Vec3Scale)(forward, forward, distance);
	FUNC_PROTO(Vec3Subtract)(out, position, forward);
}
