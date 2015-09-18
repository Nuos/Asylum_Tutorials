
#include "simplecollision.h"
#include "gl4x.h"

class RigidSphere : public RigidBody
{
private:
	float radius;

public:
	RigidSphere(float radius);

	void GetTransformWithSize(float out[16]);

	inline float GetRadius() const {
		return radius;
	}
};

class RigidBox : public RigidBody
{
private:
	float size[3];

public:
	RigidBox(float width, float height, float depth);

	void GetTransformWithSize(float out[16]);
	float RayIntersect(float normal[3], const float start[3], const float dir[3]);

	inline const float* GetSize() const {
		return size;
	}
};

// *****************************************************************************************************************************
//
// RigidSphere impl
//
// *****************************************************************************************************************************

RigidSphere::RigidSphere(float radius)
	: RigidBody(Sphere)
{
	this->radius = radius;
}

void RigidSphere::GetTransformWithSize(float out[16])
{
	memcpy(out, world, 16 * sizeof(float));

	out[0] *= radius;
	out[1] *= radius;
	out[2] *= radius;

	out[4] *= radius;
	out[5] *= radius;
	out[6] *= radius;

	out[8] *= radius;
	out[9] *= radius;
	out[10] *= radius;

	out[12] -= (pivot[0] * out[0] + pivot[1] * out[4] + pivot[2] * out[8]);
	out[13] -= (pivot[0] * out[1] + pivot[1] * out[5] + pivot[2] * out[9]);
	out[14] -= (pivot[0] * out[2] + pivot[1] * out[6] + pivot[2] * out[10]);
}

// *****************************************************************************************************************************
//
// RigidBox impl
//
// *****************************************************************************************************************************

RigidBox::RigidBox(float width, float height, float depth)
	: RigidBody(Box)
{
	GLVec3Set(size, width, height, depth);
}

void RigidBox::GetTransformWithSize(float out[16])
{
	memcpy(out, world, 16 * sizeof(float));

	out[0] *= size[0];
	out[1] *= size[0];
	out[2] *= size[0];

	out[4] *= size[1];
	out[5] *= size[1];
	out[6] *= size[1];

	out[8] *= size[2];
	out[9] *= size[2];
	out[10] *= size[2];

	out[12] -= pivot[0];
	out[13] -= pivot[1];
	out[14] -= pivot[2];
}

float RigidBox::RayIntersect(float normal[3], const float start[3], const float dir[3])
{
	OpenGLAABox bb(size);
	float s[3], d[3], p[3], q[3];
	float center[3], halfsize[3];

	GLVec3TransformCoord(s, start, worldinv);
	GLVec3TransformTranspose(d, world, dir);

	GLVec3Subtract(bb.Min, bb.Min, pivot);
	GLVec3Subtract(bb.Max, bb.Max, pivot);

	float t = bb.RayIntersect(s, d);
	float mindist = FLT_MAX;
	float dist;

	bb.GetCenter(center);
	bb.GetHalfSize(halfsize);

	GLVec3Mad(p, s, d, t);
	GLVec3Subtract(q, p, center);

	if( (dist = fabs(halfsize[0] - q[0])) < mindist ) {
		mindist = dist;
		GLVec3Set(normal, (q[0] < 0.0f ? -1.0f : 1.0f), 0, 0);
	}

	if( (dist = fabs(halfsize[1] - q[1])) < mindist ) {
		mindist = dist;
		GLVec3Set(normal, 0, (q[1] < 0.0f ? -1.0f : 1.0f), 0);
	}

	if( (dist = fabs(halfsize[2] - q[2])) < mindist ) {
		mindist = dist;
		GLVec3Set(normal, 0, 0, (q[2] < 0.0f ? -1.0f : 1.0f));
	}

	GLVec3TransformTranspose(normal, worldinv, normal);
	return t;
}

// *****************************************************************************************************************************
//
// RigidBody impl
//
// *****************************************************************************************************************************

RigidBody::RigidBody(int bodytype)
{
	type = bodytype;
	invmass = 0;
	userdata = 0;

	GLVec3Set(pivot, 0, 0, 0);
	GLVec3Set(velocity, 0, 0, 0);

	GLVec3Set(previous.position, 0, 0, 0);
	GLQuaternionIdentity(previous.orientation);

	GLVec3Set(current.position, 0, 0, 0);
	GLQuaternionIdentity(current.orientation);

	UpdateMatrices();
}

RigidBody::~RigidBody()
{
}

void RigidBody::GetVelocity(float out[3])
{
	GLVec3Assign(out, velocity);
}

void RigidBody::GetInterpolatedPosition(float out[3], float t)
{
	out[0] = (1.0f - t) * previous.position[0] + t * current.position[0];
	out[1] = (1.0f - t) * previous.position[1] + t * current.position[1];
	out[2] = (1.0f - t) * previous.position[2] + t * current.position[2];
}

void RigidBody::GetTransformWithSize(float out[16])
{
}

float RigidBody::RayIntersect(float normal[3] , const float start[3], const float dir[3])
{
	return FLT_MAX;
}

void RigidBody::UpdateMatrices()
{
	GLMatrixRotationQuaternion(prevworld, previous.orientation);
	GLMatrixRotationQuaternion(world, current.orientation);

	prevworld[12] = previous.position[0];
	prevworld[13] = previous.position[1];
	prevworld[14] = previous.position[2];

	world[12] = current.position[0];
	world[13] = current.position[1];
	world[14] = current.position[2];

	GLMatrixInverse(worldinv, world);
	GLMatrixInverse(prevworldinv, prevworld);
}

void RigidBody::Integrate(float dt)
{
	const float gravity[3] = { 0, -10, 0 };

	if( invmass == 0 )
		return;

	GLVec3Assign(previous.position, current.position);

	velocity[0] += gravity[0] * dt;
	velocity[1] += gravity[1] * dt;
	velocity[2] += gravity[2] * dt;

	current.position[0] += velocity[0] * dt;
	current.position[1] += velocity[1] * dt;
	current.position[2] += velocity[2] * dt;
}

void RigidBody::IntegratePosition(float dt)
{
	current.position[0] += velocity[0] * dt;
	current.position[1] += velocity[1] * dt;
	current.position[2] += velocity[2] * dt;
}

void RigidBody::ResolvePenetration(const Contact& contact)
{
	GLVec3Mad(current.position, current.position, contact.normal, contact.depth + 1e-3f); // 1 mm
}

void RigidBody::ResolvePenetration(float toi)
{
	GLVec3Mad(current.position, previous.position, velocity, toi);
}

void RigidBody::SetMass(float mass)
{
	if( mass == Immovable )
		invmass = 0;
	else
		invmass = 1.0f / mass;
}

void RigidBody::SetPivot(float offset[3])
{
	GLVec3Set(pivot, offset[0], offset[1], offset[2]);
}

void RigidBody::SetPosition(float x, float y, float z)
{
	GLVec3Set(previous.position, x, y, z);
	GLVec3Set(current.position, x, y, z);

	UpdateMatrices();
}

void RigidBody::SetVelocity(float x, float y, float z)
{
	GLVec3Set(velocity, x, y, z);
}

void RigidBody::SetVelocity(float v[3])
{
	GLVec3Assign(velocity, v);
}

void RigidBody::SetOrientation(float q[4])
{
	GLQuaternionSet(previous.orientation, q[0], q[1], q[2], q[3]);
	GLQuaternionSet(current.orientation, q[0], q[1], q[2], q[3]);

	UpdateMatrices();
}

// *****************************************************************************************************************************
//
// CollisionWorld impl
//
// *****************************************************************************************************************************

CollisionWorld::CollisionWorld()
{
	memset(detectors, 0, sizeof(detectors));

	detectors[1][2] = &CollisionWorld::SphereSweepBox;
	detectors[2][1] = &CollisionWorld::BoxSweepSphere;
}

CollisionWorld::~CollisionWorld()
{
	for( size_t i = 0; i < bodies.size(); ++i )
		delete bodies[i];

	bodies.clear();
}

RigidBody* CollisionWorld::AddStaticBox(float width, float height, float depth)
{
	RigidBody* body = new RigidBox(width, height, depth);

	body->SetMass(Immovable);
	bodies.push_back(body);

	return body;
}

RigidBody* CollisionWorld::AddDynamicSphere(float radius, float mass)
{
	RigidBody* body =  new RigidSphere(radius);

	body->SetMass(mass);
	bodies.push_back(body);

	return body;
}

RigidBody* CollisionWorld::RayIntersect(const float start[3], const float dir[3])
{
	float params[4];
	return RayIntersect(params, start, dir);
}

RigidBody* CollisionWorld::RayIntersect(float out[4], const float start[3], const float dir[3])
{
	RigidBody* bestbody = 0;
	float t, bestt = FLT_MAX;
	float n[3];

	// TODO: kD-tree
	for( size_t i = 0; i < bodies.size(); ++i )
	{
		t = bodies[i]->RayIntersect(n, start, dir);

		if( t < bestt )
		{
			bestt = t;
			bestbody = bodies[i];

			GLVec3Assign(out, n);
		}
	}

	out[3] = bestt;
	return bestbody;
}

bool CollisionWorld::SphereSweepBox(CollisionData& out, RigidBody* body1, RigidBody* body2)
{
	RigidSphere*	sphere		= (RigidSphere*)body1;
	RigidBox*		box			= (RigidBox*)body2;

	OpenGLAABox		inner(box->GetSize());
	Contact			contact;
	float			start[3];
	float			v1[3], v2[3];
	float			rel_vel[3];

	contact.body1 = body1;
	contact.body2 = body2;

	// calculate relative velocity
	GLVec3Subtract(v1, box->current.position, box->previous.position);
	GLVec3Subtract(v2, sphere->current.position, sphere->previous.position);
	GLVec3Subtract(rel_vel, v2, v1);

	if( GLVec3Dot(rel_vel, rel_vel) < 1e-5f )
		return false;

	// transform to box space
	GLVec3TransformCoord(start, sphere->previous.position, box->worldinv);
	GLVec3Transform(rel_vel, rel_vel, box->worldinv);

	GLVec3Subtract(inner.Min, inner.Min, box->pivot);
	GLVec3Subtract(inner.Max, inner.Max, box->pivot);

	float	estpos[3];
	float	closest[3];
	float	worldplane[4];
	float	norm[3];
	float	tmp[3];
	float	dist, prevdist;
	float	maxdist = GLVec3Length(rel_vel);
	float	dt, t = 0;
	int		numiter = 0;

	GLVec3Assign(estpos, start);

	closest[0] = GLClamp(estpos[0], inner.Min[0], inner.Max[0]);
	closest[1] = GLClamp(estpos[1], inner.Min[1], inner.Max[1]);
	closest[2] = GLClamp(estpos[2], inner.Min[2], inner.Max[2]);

	dist = GLVec3Distance(estpos, closest) - sphere->GetRadius();

	if( maxdist > 1e-3f )
	{
		// conservative advancement
		while( numiter < 15 )
		{
			prevdist = dist;
			dt = dist / maxdist;

			GLVec3Scale(tmp, rel_vel, dt);
			GLVec3Add(estpos, estpos, tmp);

			t += dt;

			closest[0] = GLClamp(estpos[0], inner.Min[0], inner.Max[0]);
			closest[1] = GLClamp(estpos[1], inner.Min[1], inner.Max[1]);
			closest[2] = GLClamp(estpos[2], inner.Min[2], inner.Max[2]);

			dist = GLVec3Distance(estpos, closest) - sphere->GetRadius();

			if( dist >= prevdist || t > 1 || dist < 1e-3f ) // 1 mm
				break;

			++numiter;
		}
	}

	if( dist < 1e-3f )
	{
		const float planes[6][4] =
		{
			{ 1, 0, 0, -inner.Max[0] },
			{ -1, 0, 0, -inner.Min[0] },
			{ 0, 1, 0, -inner.Max[1]},
			{ 0, -1, 0, -inner.Min[1]},
			{ 0, 0, 1, -inner.Max[2]},
			{ 0, 0, -1, -inner.Min[2]},
		};

		GLVec3Subtract(norm, estpos, closest);

		if( GLVec3Dot(norm, norm) < 1e-5f )
		{
			// find plane by hand
			maxdist = -FLT_MAX;

			for( int i = 0; i < 6; ++i )
			{
				dist = GLPlaneDistance(planes[i], estpos);

				if( fabs(dist) < sphere->GetRadius() )
				{
					GLVec4Assign(worldplane, planes[i]);
					break;
				}
			}
		}
		else
			GLPlaneFromRay(worldplane, closest, norm);

		GLVec4TransformTranspose(worldplane, box->worldinv, worldplane);
		GLPlaneNormalize(worldplane, worldplane);

		contact.toi = t;

		// convert to current frame
		t = GLPlaneDistance(worldplane, sphere->current.position);
		contact.depth = sphere->GetRadius() - t;

		GLVec3Assign(contact.normal, worldplane);

		// contact on sphere
		GLVec3Scale(contact.pos1, contact.normal, sphere->GetRadius());
		GLVec3Subtract(contact.pos1, sphere->current.position, contact.pos1);

		// contact on box
		GLVec3Scale(contact.pos2, contact.normal, t);
		GLVec3Subtract(contact.pos2, sphere->current.position, contact.pos2);

		out.contacts.push_back(contact);
		return true;
	}

	return false;
}

bool CollisionWorld::BoxSweepSphere(CollisionData& out, RigidBody* body1, RigidBody* body2)
{
	bool collided = SphereSweepBox(out, body2, body1);

	if( out.contacts.size() > 0 )
	{
		Contact& contact = out.contacts[0];

		GLVec3Scale(contact.normal, contact.normal, -1);
		GLVec3Swap(contact.pos1, contact.pos2);

		std::swap(contact.body1, contact.body2);
	}

	return collided;
}

bool CollisionWorld::Detect(CollisionData& out, RigidBody* body1, RigidBody* body2)
{
	DetectorFunc func = detectors[body1->GetType()][body2->GetType()];

	if( func )
		return (this->*func)(out, body1, body2);

	return false;
}

void CollisionWorld::DetectCollisions(CollisionData& out, RigidBody* body)
{
	for( size_t i = 0; i < bodies.size(); ++i )
	{
		if( bodies[i] != body )
			Detect(out, body, bodies[i]);
	}
}

void CollisionWorld::DEBUG_Visualize(void (*callback)(RigidBody::BodyType, float[16]))
{
	if( !callback )
		return;

	float xform[16];

	for( size_t i = 0; i < bodies.size(); ++i )
	{
		bodies[i]->GetTransformWithSize(xform);
		(*callback)(bodies[i]->GetType(), xform);
	}
}
