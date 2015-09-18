
#include "fpscamera.h"
#include "simplecollision.h"

#define MOVEMENT_SPEED			1.4f	// m/s
#define ROTATIONAL_SPEED		0.75f	// rad/s
#define ROTATIONAL_INVINTERTIA	5.0f
#define CAMERA_RADIUS			0.25f

FPSCamera::FPSCamera(CollisionWorld* world)
{
	Aspect		= 4.0f / 3.0f;
	Fov			= GLDegreesToRadians(80);
	Near		= 0.1f;
	Far			= 50.0f;

	state		= 0;
	body		= 0;
	collworld	= world;
	isonground	= false;

	array_state_set(anglecurve, 0, 0, 0);

	GLVec3Set(position, 0, 1.8f, 0);
	GLVec3Set(targetangles, 0, 0, 0);
	GLVec3Set(smoothedangles, 0, 0, 0);

	GLMatrixIdentity(view);

	if( world )
	{
		body = world->AddDynamicSphere(CAMERA_RADIUS, 80);
		body->SetPosition(0, 0.1f, 0);
	}
}

void FPSCamera::FitToBox(const OpenGLAABox& box)
{
	float look[3];

	GLVec3Set(look, -view[2], -view[6], -view[10]);
	GLVec3Add(look, position, look);

	GLFitToBox(Near, Far, position, look, box);
}

void FPSCamera::GetEyePosition(float out[3])
{
	out[0] = position[0];
	out[1] = position[1];
	out[2] = position[2];
}

void FPSCamera::GetViewMatrix(float out[16])
{
	memcpy(out, view, 16 * sizeof(float));
}

void FPSCamera::GetViewVectors(float forward[3], float right[3], float up[3])
{
	float yaw[16];
	float pitch[16];

	// TODO: rollpitchyaw
	GLMatrixRotationAxis(yaw, anglecurve.curr[0], 0, 1, 0);
	GLMatrixRotationAxis(pitch, anglecurve.curr[1], 1, 0, 0);

	GLMatrixMultiply(view, yaw, pitch);

	GLVec3Set(forward, -view[2], -view[6], -view[10]);
	GLVec3Set(right, view[0], view[4], view[8]);
	GLVec3Set(up, view[1], view[5], view[9]);
}

void FPSCamera::GetProjectionMatrix(float out[16])
{
	GLMatrixPerspectiveRH(out, Fov, Aspect, Near, Far);
}

void FPSCamera::SetEyePosition(float x, float y, float z)
{
	GLVec3Set(position, x, y, z);

	if( body )
		body->SetPosition(x, y - (1.8f - CAMERA_RADIUS), z);
}

void FPSCamera::SetOrientation(float yaw, float pitch, float roll)
{
	GLVec3Set(targetangles, yaw, pitch, roll);
	GLVec3Set(smoothedangles, yaw, pitch, roll);

	array_state_set(anglecurve, yaw, pitch, roll);
}

void FPSCamera::Update(float dt)
{
	float forward[3], right[3], up[3];
	float diff[3];
	float movedir[2] = { 0, 0 };

	// rotate
	targetangles[1] = GLClamp(targetangles[1], -GL_HALF_PI, GL_HALF_PI);

	diff[0] = (targetangles[0] - anglecurve.curr[0]) * dt * ROTATIONAL_INVINTERTIA;
	diff[1] = (targetangles[1] - anglecurve.curr[1]) * dt * ROTATIONAL_INVINTERTIA;
	diff[2] = 0;

	anglecurve.extend(diff);
	
	// move
	GLVec3Set(diff, 0, 0, 0);

	if( state & State_Moving )
	{
		if( state & State_Left )
			movedir[0] = -1;

		if( state & State_Right )
			movedir[0] = 1;

		if( state & State_Forward )
			movedir[1] = 1;

		if( state & State_Backward )
			movedir[1] = -1;

		GetViewVectors(forward, right, up);

		if( forward[1] > 0.98f )
			GLVec3Set(forward, -up[0], -up[1], -up[2]);

		if( forward[1] < -0.98f )
			GLVec3Set(forward, up[0], up[1], up[2]);

		forward[1] = right[1] = 0;

		GLVec3Scale(forward, forward, movedir[1]);
		GLVec3Scale(right, right, movedir[0]);
		GLVec3Add(diff, forward, right);

		GLVec3Normalize(diff, diff);
		GLVec3Scale(diff, diff, MOVEMENT_SPEED);
	}

	// update body (NOTE: don't even try it with physics)
	CollisionData	data;
	RigidBody*		groundbody = 0;
	float			groundplane[4];
	float			hitparams[4];
	float			hitpos[3];
	float			prevpos[3];
	float			vel[3];
	float			deltavel[3] = { 0, 0, 0 };
	float			down[3] = { 0, -1, 0 };
	bool			wasonground = isonground;

	GLVec3Assign(prevpos, body->GetPosition());

	if( wasonground )
		body->SetVelocity(diff);
	
	body->Integrate(dt);

	// look for ground first
	groundbody = collworld->RayIntersect(hitparams, prevpos, down);
	isonground = false;

	if( groundbody && hitparams[3] >= 0 ) // && hitparams[1] > 0.64f
	{
		GLVec3Mad(hitpos, prevpos, down, hitparams[3]);
		GLPlaneFromRay(groundplane, hitpos, hitparams);
		GLVec3Subtract(vel, body->GetPosition(), prevpos);

		hitparams[3] = (CAMERA_RADIUS - groundplane[3] - GLVec3Dot(hitparams, prevpos)) / GLVec3Dot(hitparams, vel);

		if( hitparams[3] > -0.1f && hitparams[3] < 1.0f )
		{
			// resolve position
			body->ResolvePenetration(hitparams[3] * dt);
			isonground = true;

			// resolve velocity and integrate
			float length = GLVec3Length(diff);
			float cosa = GLVec3Dot(diff, hitparams);

			GLVec3Mad(diff, diff, hitparams, -cosa);

			if( fabs(length) > 1e-5f )
				GLVec3Scale(diff, diff, length / GLVec3Length(diff));

			body->SetVelocity(diff);
			body->IntegratePosition(dt);
		}
	}

	body->GetVelocity(vel);

	// now test every other object
	collworld->DetectCollisions(data, body);

	for( size_t i = 0; i < data.contacts.size(); ++i )
	{
		const Contact& contact = data.contacts[i];

		if( contact.body2 == groundbody )
			continue;

		if( contact.depth > 0 )
		{
			body->ResolvePenetration(contact);

			float rel_vel = GLVec3Dot(contact.normal, vel);
			float impulse = rel_vel;

			GLVec3Mad(deltavel, deltavel, contact.normal, -impulse);
		}
	}

	GLVec3Add(vel, vel, deltavel);
	body->SetVelocity(vel);
}

void FPSCamera::Animate(float alpha)
{
	anglecurve.smooth(smoothedangles, alpha);
	body->GetInterpolatedPosition(position, alpha);

	position[1] += (1.8f - CAMERA_RADIUS);

	// recalculate view matrix
	float yaw[16];
	float pitch[16];

	// TODO: rollpitchyaw
	GLMatrixRotationAxis(yaw, smoothedangles[0], 0, 1, 0);
	GLMatrixRotationAxis(pitch, smoothedangles[1], 1, 0, 0);

	GLMatrixMultiply(view, yaw, pitch);

	view[12] = -(position[0] * view[0] + position[1] * view[4] + position[2] * view[8]);
	view[13] = -(position[0] * view[1] + position[1] * view[5] + position[2] * view[9]);
	view[14] = -(position[0] * view[2] + position[1] * view[6] + position[2] * view[10]);
}

void FPSCamera::Event_KeyDown(unsigned char keycode)
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

void FPSCamera::Event_KeyUp(unsigned char keycode)
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

void FPSCamera::Event_MouseMove(short dx, short dy)
{
	if( state & State_Rotating )
	{
		float speedx = GLDegreesToRadians((float)dx);
		float speedy = GLDegreesToRadians((float)dy);

		targetangles[0] += speedx * ROTATIONAL_SPEED;
		targetangles[1] += speedy * ROTATIONAL_SPEED;
	}
}

void FPSCamera::Event_MouseDown(unsigned char button)
{
	if( button & 1 )
		state |= State_Rotating;
}

void FPSCamera::Event_MouseUp(unsigned char button)
{
	if( button & 1 )
		state &= (~State_Rotating);
}
