
#include "basiccamera.h"

#define ROTATIONAL_SPEED		0.75f	// rad/s
#define ROTATIONAL_INVINTERTIA	5.0f
#define POSITIONAL_INVINTERTIA	5.0f

BasicCamera::BasicCamera()
{
	distance	= 1.0f;
	nearplane	= 0.1f;
	farplane	= 50.0f;
	fov			= FUNC_PROTO(_PI) / 3;
	aspect		= 4.0f / 3.0f;
	finished	= true;
	
	array_state_set(anglecurve, 0, 0, 0);
	array_state_set(pancurve, 0, 0, 0);

	FUNC_PROTO(Vec3Set)(position, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(targetangles, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(smoothedangles, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(targetpan, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(position, 0, 0, 0);
}

void BasicCamera::OrbitRight(float angle)
{
	targetangles[0] += angle * ROTATIONAL_SPEED;
	finished = false;
}

void BasicCamera::OrbitUp(float angle)
{
	targetangles[1] += angle * ROTATIONAL_SPEED;
	targetangles[1] = FUNC_PROTO(Clamp)(targetangles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));

	finished = false;
}

void BasicCamera::PanRight(float offset)
{
	float view[16];
	float yaw[16];
	float pitch[16];
	float right[3];

	FUNC_PROTO(MatrixRotationAxis)(yaw, smoothedangles[0], 0, 1, 0);
	FUNC_PROTO(MatrixRotationAxis)(pitch, smoothedangles[1], 1, 0, 0);
	FUNC_PROTO(MatrixMultiply)(view, yaw, pitch);

	FUNC_PROTO(Vec3Set)(right, view[0], view[4], view[8]);
	FUNC_PROTO(Vec3Mad)(targetpan, targetpan, right, offset);
	//FUNC_PROTO(Vec3Mad)(position, position, right, offset);
}

void BasicCamera::PanUp(float offset)
{
	float view[16];
	float yaw[16];
	float pitch[16];
	float up[3];

	FUNC_PROTO(MatrixRotationAxis)(yaw, smoothedangles[0], 0, 1, 0);
	FUNC_PROTO(MatrixRotationAxis)(pitch, smoothedangles[1], 1, 0, 0);
	FUNC_PROTO(MatrixMultiply)(view, yaw, pitch);

	FUNC_PROTO(Vec3Set)(up, view[1], view[5], view[9]);
	FUNC_PROTO(Vec3Mad)(targetpan, targetpan, up, offset);
	//FUNC_PROTO(Vec3Mad)(position, position, up, offset);
}

void BasicCamera::GetViewAndEye(float view[16], float eye[3]) const
{
	float yaw[16];
	float pitch[16];
	float forward[3];

	FUNC_PROTO(MatrixRotationAxis)(yaw, smoothedangles[0], 0, 1, 0);
	FUNC_PROTO(MatrixRotationAxis)(pitch, smoothedangles[1], 1, 0, 0);
	FUNC_PROTO(MatrixMultiply)(view, yaw, pitch);

	// rotation matrix is right handed
	FUNC_PROTO(Vec3Set)(forward, -view[2], -view[6], -view[10]);
	FUNC_PROTO(Vec3Scale)(forward, forward, distance);
	FUNC_PROTO(Vec3Subtract)(eye, position, forward);
}

void BasicCamera::GetViewMatrix(float out[16]) const
{
	float eye[3];
	GetViewAndEye(out, eye);

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
	float view[16];
	GetViewAndEye(view, out);
}

void BasicCamera::Update(float dt)
{
	float diff1[3];
	float diff2[3];

	// rotate
	targetangles[1] = FUNC_PROTO(Clamp)(targetangles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));

	diff1[0] = (targetangles[0] - anglecurve.curr[0]) * dt * ROTATIONAL_INVINTERTIA;
	diff1[1] = (targetangles[1] - anglecurve.curr[1]) * dt * ROTATIONAL_INVINTERTIA;
	diff1[2] = 0;

	anglecurve.extend(diff1);

	// pan
	diff2[0] = (targetpan[0] - pancurve.curr[0]) * dt * POSITIONAL_INVINTERTIA;
	diff2[1] = (targetpan[1] - pancurve.curr[1]) * dt * POSITIONAL_INVINTERTIA;
	diff2[2] = (targetpan[2] - pancurve.curr[2]) * dt * POSITIONAL_INVINTERTIA;

	pancurve.extend(diff2);

	if( FUNC_PROTO(Vec3Dot)(diff1, diff1) < 1e-4f && FUNC_PROTO(Vec3Dot)(diff2, diff2) < 1e-4f )
		finished = true;
}

void BasicCamera::Animate(float alpha)
{
	anglecurve.smooth(smoothedangles, alpha);
	pancurve.smooth(position, alpha);
}

void BasicCamera::SetOrientation(float yaw, float pitch, float roll)
{
	FUNC_PROTO(Vec3Set)(targetangles, yaw, pitch, roll);
	FUNC_PROTO(Vec3Set)(smoothedangles, yaw, pitch, roll);

	array_state_set(anglecurve, yaw, pitch, roll);
	finished = true;
}

void BasicCamera::SetPosition(float x, float y, float z)
{
	FUNC_PROTO(Vec3Set)(targetpan, x, y, z);
	FUNC_PROTO(Vec3Set)(position, x, y, z);

	array_state_set(pancurve, x, y, z);
	finished = true;
}
