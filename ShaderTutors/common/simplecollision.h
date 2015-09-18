
#ifndef _SIMPLECOLLISION_H_
#define _SIMPLECOLLISION_H_

#include <vector>

const float Immovable = 3.402823466e+38f;

class RigidBody;

struct Contact
{
	float		pos1[3];
	float		pos2[3];
	float		normal[3];	// on body2
	RigidBody*	body1;
	RigidBody*	body2;
	float		depth;
	float		toi;
};

struct CollisionData
{
	std::vector<Contact> contacts;
};

class RigidBody
{
	friend class CollisionWorld;

	struct Internal_State
	{
		float	orientation[4];
		float	position[3];
	};

protected:
	Internal_State	previous;
	Internal_State	current;

	float	prevworld[16];
	float	prevworldinv[16];
	float	world[16];
	float	worldinv[16];

	float	velocity[3];
	float	pivot[3];
	float	invmass;
	void*	userdata;
	int		type;

	RigidBody(int bodytype);

	void UpdateMatrices();

public:
	enum BodyType
	{
		None = 0,
		Sphere = 1,
		Box = 2
	};

	virtual ~RigidBody();

	virtual void GetTransformWithSize(float out[16]);
	virtual float RayIntersect(float normal[3], const float start[3], const float dir[3]);

	void GetInterpolatedPosition(float out[3], float t);
	void GetVelocity(float out[3]);
	
	void Integrate(float dt);
	void IntegratePosition(float dt);
	void ResolvePenetration(const Contact& contact);
	void ResolvePenetration(float toi);

	void SetMass(float mass);
	void SetPivot(float offset[3]);
	void SetPosition(float x, float y, float z);
	void SetVelocity(float x, float y, float z);
	void SetVelocity(float v[3]);
	void SetOrientation(float q[4]);

	inline void SetUserData(void* data) {
		userdata = data;
	}

	inline void* GetUserData() {
		return userdata;
	}

	inline const float* GetPosition() const {
		return current.position;
	}

	inline const float* GetTransform() const {
		return world;
	}

	inline const float* GetInverseTransform() const {
		return worldinv;
	}

	inline BodyType GetType() const {
		return (BodyType)type;
	}
};

class CollisionWorld
{
	typedef std::vector<RigidBody*> BodyList;

	typedef bool (CollisionWorld::*DetectorFunc)(CollisionData&, RigidBody*, RigidBody*);

private:
	DetectorFunc	detectors[3][3];
	BodyList		bodies;

	bool SphereSweepBox(CollisionData& out, RigidBody* body1, RigidBody* body2);
	bool BoxSweepSphere(CollisionData& out, RigidBody* body1, RigidBody* body2);
	bool Detect(CollisionData& out, RigidBody* body1, RigidBody* body2);

public:
	CollisionWorld();
	~CollisionWorld();

	RigidBody* AddStaticBox(float width, float height, float depth);
	RigidBody* AddDynamicSphere(float radius, float mass);
	RigidBody* RayIntersect(const float start[3], const float dir[3]);
	RigidBody* RayIntersect(float out[4], const float start[3], const float dir[3]);

	void DetectCollisions(CollisionData& out, RigidBody* body);

	void DEBUG_Visualize(void (*callback)(RigidBody::BodyType, float[16]));
};

#endif
