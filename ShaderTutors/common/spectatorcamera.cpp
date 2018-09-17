
#include "spectatorcamera.h"

#define MOVEMENT_SPEED			1.4f	// m/s
#define ROTATIONAL_SPEED		0.75f	// rad/s
#define ROTATIONAL_INVINTERTIA	5.0f

SpectatorCamera::SpectatorCamera()
{
	Aspect		= 4.0f / 3.0f;
	Fov			= FUNC_PROTO(DegreesToRadians)(80);
	Near		= 0.1f;
	Far			= 50.0f;
	Speed		= MOVEMENT_SPEED;

	state = 0;
	finished = true;

	array_state_set(anglecurve, 0, 0, 0);
	array_state_set(positioncurve, 0, 1.8f, 0);

	FUNC_PROTO(Vec3Set)(targetangles, 0, 0, 0);
	FUNC_PROTO(Vec3Set)(smoothedangles, 0, 0, 0);
	FUNC_PROTO(Vec3Assign)(smoothedposition, positioncurve.curr);

	FUNC_PROTO(MatrixIdentity)(view);
}

void SpectatorCamera::FitToBox(const CLASS_PROTO(AABox)& box)
{
	float look[3];

	FUNC_PROTO(Vec3Set)(look, view[2], view[6], view[10]);
	FUNC_PROTO(Vec3Add)(look, smoothedposition, look);
	FUNC_PROTO(FitToBox)(Near, Far, smoothedposition, look, box);
}

void SpectatorCamera::GetEyePosition(float out[3])
{
	FUNC_PROTO(Vec3Assign)(out, smoothedposition);
}

void SpectatorCamera::GetOrientation(float out[3])
{
	FUNC_PROTO(Vec3Assign)(out, smoothedangles);
}

void SpectatorCamera::GetViewMatrix(float out[16])
{
	FUNC_PROTO(MatrixAssign)(out, view);
}

void SpectatorCamera::GetViewVectors(float forward[3], float right[3], float up[3])
{
	float yaw[16];
	float pitch[16];

	FUNC_PROTO(MatrixRotationAxis)(yaw, anglecurve.curr[0], 0, 1, 0);
	FUNC_PROTO(MatrixRotationAxis)(pitch, anglecurve.curr[1], 1, 0, 0);
	FUNC_PROTO(MatrixMultiply)(view, yaw, pitch);

	// rotation matrix is right-handed
	FUNC_PROTO(Vec3Set)(forward, -view[2], -view[6], -view[10]);
	FUNC_PROTO(Vec3Set)(right, view[0], view[4], view[8]);
	FUNC_PROTO(Vec3Set)(up, view[1], view[5], view[9]);
}

void SpectatorCamera::GetProjectionMatrix(float out[16])
{
	FUNC_PROTO(MatrixPerspectiveFovRH)(out, Fov, Aspect, Near, Far);
}

void SpectatorCamera::SetEyePosition(float x, float y, float z)
{
	FUNC_PROTO(Vec3Set)(smoothedposition, x, y, z);

	array_state_set(positioncurve, x, y, z);
	finished = false;
}

void SpectatorCamera::SetOrientation(float yaw, float pitch, float roll)
{
	FUNC_PROTO(Vec3Set)(targetangles, yaw, pitch, roll);
	FUNC_PROTO(Vec3Set)(smoothedangles, yaw, pitch, roll);

	array_state_set(anglecurve, yaw, pitch, roll);
	finished = false;
}

void SpectatorCamera::Update(float dt)
{
	float forward[3], right[3], up[3];
	float diff[3];
	float movedir[2] = { 0, 0 };

	// rotate
	targetangles[1] = FUNC_PROTO(Clamp)(targetangles[1], -FUNC_PROTO(_HALF_PI), FUNC_PROTO(_HALF_PI));

	diff[0] = (targetangles[0] - anglecurve.curr[0]) * dt * ROTATIONAL_INVINTERTIA;
	diff[1] = (targetangles[1] - anglecurve.curr[1]) * dt * ROTATIONAL_INVINTERTIA;
	diff[2] = 0;

	if( state != 0 )
		finished = false;
	else if( FUNC_PROTO(Vec3Dot)(diff, diff) < 1e-8f )
		finished = true;

	anglecurve.extend(diff);
	
	// move
	FUNC_PROTO(Vec3Set)(diff, 0, 0, 0);

	if( state & State_Moving ) {
		if( state & State_Left )
			movedir[0] = -1;

		if( state & State_Right )
			movedir[0] = 1;

		if( state & State_Forward )
			movedir[1] = 1;

		if( state & State_Backward )
			movedir[1] = -1;

		GetViewVectors(forward, right, up);

		FUNC_PROTO(Vec3Scale)(forward, forward, movedir[1]);
		FUNC_PROTO(Vec3Scale)(right, right, movedir[0]);
		FUNC_PROTO(Vec3Add)(diff, forward, right);

		FUNC_PROTO(Vec3Normalize)(diff, diff);
		FUNC_PROTO(Vec3Scale)(diff, diff, dt * Speed);
	}

	positioncurve.extend(diff);
}

void SpectatorCamera::Animate(float alpha)
{
	float yaw[16];
	float pitch[16];

	anglecurve.smooth(smoothedangles, alpha);
	positioncurve.smooth(smoothedposition, alpha);

	// recalculate view matrix
	FUNC_PROTO(MatrixRotationAxis)(yaw, smoothedangles[0], 0, 1, 0);
	FUNC_PROTO(MatrixRotationAxis)(pitch, smoothedangles[1], 1, 0, 0);
	FUNC_PROTO(MatrixMultiply)(view, yaw, pitch);

	view[12] = -(smoothedposition[0] * view[0] + smoothedposition[1] * view[4] + smoothedposition[2] * view[8]);
	view[13] = -(smoothedposition[0] * view[1] + smoothedposition[1] * view[5] + smoothedposition[2] * view[9]);
	view[14] = -(smoothedposition[0] * view[2] + smoothedposition[1] * view[6] + smoothedposition[2] * view[10]);
}

void SpectatorCamera::Event_KeyDown(uint8_t keycode)
{
	if( keycode == 0x57 ) // W
		state |= State_Forward;

	if( keycode == 0x53 ) // S
		state |= State_Backward;

	if( keycode == 0x44 ) // D
		state |= State_Right;

	if( keycode == 0x41 ) // A
		state |= State_Left;
}

void SpectatorCamera::Event_KeyUp(uint8_t keycode)
{
	if( keycode == 0x57 ) // W
		state &= (~State_Forward);

	if( keycode == 0x53 ) // S
		state &= (~State_Backward);

	if( keycode == 0x44 ) // D
		state &= (~State_Right);

	if( keycode == 0x41 ) // A
		state &= (~State_Left);
}

void SpectatorCamera::Event_MouseMove(int16_t dx, int16_t dy)
{
	if( state & State_Rotating ) {
		float speedx = FUNC_PROTO(DegreesToRadians)((float)dx);
		float speedy = FUNC_PROTO(DegreesToRadians)((float)dy);

		targetangles[0] += speedx * ROTATIONAL_SPEED;
		targetangles[1] += speedy * ROTATIONAL_SPEED;
	}
}

void SpectatorCamera::Event_MouseDown(uint8_t button)
{
	if( button & 1 )
		state |= State_Rotating;
}

void SpectatorCamera::Event_MouseUp(uint8_t button)
{
	if( button & 1 )
		state &= (~State_Rotating);
}
