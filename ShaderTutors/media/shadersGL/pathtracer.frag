#version 150

#define SCENE			3

#define FLT_MAX			3.40282347E+38
#define PI				3.1415926535897932
#define PI_DOUBLE		6.2831853071795864
#define ONE_OVER_PI		0.3183098861837906

#ifndef FLT_MAX
#define FLT_MAX			3.402823466e+38
#endif

#if SCENE == 1
#define NUM_OBJECTS		20 //21
#elif SCENE == 2
#define NUM_OBJECTS		5
#elif SCENE == 3
#define NUM_OBJECTS		7
#endif

uniform sampler2D prevIteration;

uniform mat4	matViewProjInv;
uniform vec3	eyePos;
uniform float	time;
uniform float	currSample;

in vec4 cpos;

#ifdef RENDER_GBUFFER
uniform mat4	matView;
uniform mat4	matViewInv;
uniform vec2	clipPlanes;

out vec4 my_FragColor0;
out vec4 my_FragColor1;
out float my_FragColor2;
#else
out vec4 my_FragColor0;
#endif

struct SceneObject
{
	int		type;		// 1 -> plane, 2 -> sphere, 3 -> box, 4 -> cylinder
	vec4	params1;
	vec4	params2;
	vec3	color;
};

// =======================================================================
//
// Path tracing functions
//
// =======================================================================

float Random(vec3 pixel, vec3 scale, float seed)
{
	return fract(sin(dot(pixel + seed, scale)) * 43758.5453 + seed);
}

float RayIntersectPlane(out vec3 n, vec4 p, vec3 start, vec3 dir)
{
	float u = (dir.x * p.x + dir.y * p.y + dir.z * p.z);
	float t = 0;

	n = p.xyz;

	if( u < -1e-5 )
		t = -(start.x * p.x + start.y * p.y + start.z * p.z + p.w) / u;

	return ((t > 0.0f) ? t : FLT_MAX);
}

float RayIntersectSphere(out vec3 n, vec3 center, float radius, vec3 start, vec3 dir)
{
	vec3 stoc = start - center;

	float a = dot(dir, dir);
	float b = 2.0 * dot(stoc, dir);
	float c = dot(stoc, stoc) - radius * radius;
	float d = b * b - 4.0 * a * c;
	float t = 0.0;

	if( d > 0.0 )
		t = (-b - sqrt(d)) / (2.0 * a);

	n = normalize(start + t * dir - center);

	return ((t > 0.0) ? t : FLT_MAX);
}

float RayIntersectBox(out vec3 n, vec3 pos, vec3 size, vec3 start, vec3 dir)
{
	vec3 hsize = size * 0.5;
	vec3 bmin = pos - hsize;
	vec3 bmax = pos + hsize;

	vec3 m1 = bmin - start;
	vec3 m2 = bmax - start;

	vec3 tmin = m1 / dir;
	vec3 tmax = m2 / dir;

	vec3 t1 = min(tmin, tmax);
	vec3 t2 = max(tmin, tmax);

	float tn = max(max(t1.x, t1.y), t1.z);
	float tf = min(min(t2.x, t2.y), t2.z);

	float t = FLT_MAX;

	if( tn < tf && tf > 0.0 )
		t = tn;

	vec3 p = start + (t - 1e-3) * dir;

	if( p.x < bmin.x + 1e-4 )
		n = vec3(-1.0, 0.0, 0.0);
	else if( p.x > bmax.x - 1e-4 )
		n = vec3(1.0, 0.0, 0.0);
	else if( p.y < bmin.y + 1e-4 )
		n = vec3(0.0, -1.0, 0.0);
	else if( p.y > bmax.y - 1e-4 )
		n = vec3(0.0, 1.0, 0.0);
	else if( p.z < bmin.z + 1e-4 )
		n = vec3(0.0, 0.0, -1.0);
	else
		n = vec3(0.0, 0.0, 1.0);

	return t;
}

float RayIntersectDisk(out vec3 n, vec3 pos, vec3 axis, float radius, vec3 start, vec3 dir)
{
	vec4 p = vec4(axis, -dot(pos, axis));
	float t = RayIntersectPlane(n, p, start, dir);

	if( t != FLT_MAX ) {
		vec3 y = start + t * dir - pos;

		if( dot(y, y) > radius * radius )
			t = FLT_MAX;
	}

	return ((t > 0.0) ? t : FLT_MAX);
}

float RayIntersectCylinder(out vec3 n, vec4 pos, vec4 axis, vec3 start, vec3 dir)
{
	float radius = pos.w;
	float halfheight = axis.w * 0.5;

	vec3 x = cross(axis.xyz, dir);
	vec3 y = cross(axis.xyz, start - pos.xyz);

	float a = dot(x, x);
	float b = 2.0 * dot(x, y);
	float c = dot(y, y) - radius * radius;
	float d = b * b - 4.0 * a * c;
	float t = 0.0;
	float test;

	if( d > 0.0 )
		t = (-b - sqrt(d)) / (2.0 * a);

	x = start + t * dir;
	test = dot(x - pos.xyz, axis.xyz);

	if( abs(test) > halfheight ) {
		t = RayIntersectDisk(n, pos.xyz + halfheight * axis.xyz, axis.xyz, radius, start, dir);

		if( t == FLT_MAX )
			t = RayIntersectDisk(n, pos.xyz - halfheight * axis.xyz, -axis.xyz, radius, start, dir);
	} else {
		y = cross(x - pos.xyz, axis.xyz);
		n = normalize(cross(axis.xyz, y));
	}

	return ((t > 0.0) ? t : FLT_MAX);
}

vec3 CosineSample(vec3 n, vec3 pixel, float seed)
{
	float u = Random(pixel, vec3(12.9898, 78.233, 151.7182), seed);
	float v = Random(pixel, vec3(63.7264, 10.873, 623.6736), seed);

	float phi = 2 * PI * u;
	float costheta = sqrt(v);
	float sintheta = sqrt(1 - costheta * costheta);

	vec3 H;

	H.x = sintheta * cos(phi);
	H.y = sintheta * sin(phi);
	H.z = costheta;

	vec3 up = ((abs(n.z) < 0.999) ? vec3(0, 0, 1) : vec3(1, 0, 0));
	vec3 tangent = normalize(cross(up, n));
	vec3 bitangent = cross(n, tangent);

	return tangent * H.x + bitangent * H.y + n * H.z;
}

int FindIntersection(out vec3 pos, out vec3 norm, vec3 raystart, vec3 raydir, SceneObject objects[NUM_OBJECTS])
{
	vec3	bestn, n;
	float	t, bestt	= FLT_MAX;
	int		index		= NUM_OBJECTS;
	int		i;

	// find first object that the ray hits
	for( i = 0; i < NUM_OBJECTS; ++i ) {
		if( objects[i].type == 1 )
			t = RayIntersectPlane(n, objects[i].params1, raystart, raydir);
		else if( objects[i].type == 2 )
			t = RayIntersectSphere(n, objects[i].params1.xyz, objects[i].params1.w, raystart, raydir);
		else if( objects[i].type == 3 )
			t = RayIntersectBox(n, objects[i].params1.xyz, objects[i].params2.xyz, raystart, raydir);
		else
			t = RayIntersectCylinder(n, objects[i].params1, objects[i].params2, raystart, raydir);

		if( t < bestt ) {
			bestt	= t;
			bestn	= n;
			index	= i;
		}
	}

	if( index < NUM_OBJECTS ) {
		pos = raystart + (bestt - 1e-3) * raydir;
		norm = bestn;
	}

	return index;
}

vec3 TraceScene(vec3 raystart, vec3 raydir, vec3 pixel, SceneObject objects[NUM_OBJECTS])
{
	vec3	inray;
	vec3	outray;
	vec3	n, p, q;
	vec3	otherp;
	vec3	indirect = vec3(1.0);
	int		index;

	p = raystart;
	outray = raydir;

	index = FindIntersection(p, n, p, raydir, objects);

	if( index < NUM_OBJECTS ) {
		inray = CosineSample(n, pixel, time);
		index = FindIntersection(q, n, p, inray, objects);

		if( index < NUM_OBJECTS )
			indirect = vec3(0.0);
	} else {
		return vec3(0.0, 0.0103, 0.0707);
	}

	return indirect;
}

// =======================================================================
//
// Shader
//
// =======================================================================

void main()
{
	SceneObject objects[NUM_OBJECTS] = SceneObject[NUM_OBJECTS](
#if SCENE == 1
		SceneObject(2, vec4(-1.155, 0.510, -1.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-1.155, 0.510, -0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-1.155, 0.510, 0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-1.155, 0.510, 1.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.289, 0.510, -1.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.289, 0.510, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.289, 0.510, 1.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(0.577, 0.510, -0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(0.577, 0.510, 0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(1.443, 0.510, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.866, 1.326, -1.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.866, 1.326, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.866, 1.326, 1.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.000, 1.326, -0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.000, 1.326, 0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(0.866, 1.326, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.577, 2.143, -0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.577, 2.143, 0.500, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(0.289, 2.143, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1)),
		SceneObject(2, vec4(-0.289, 2.959, 0.000, 0.5), vec4(0.0), vec3(1.0, 0.3, 0.1))

#if NUM_OBJECTS == 21
		// ground
		, SceneObject(1, vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0), vec3(0.664, 0.824, 0.85))
#endif

#elif SCENE == 2
		SceneObject(3, vec4(0.2, 1.5, 1.5, 0.0), vec4(5.0, 3.0, 1.0, 0.0), vec3(1.0)),
		SceneObject(3, vec4(3.2, 2.0, 0.0, 0.0), vec4(1.0, 4.0, 5.0, 0.0), vec3(1.0)),
		SceneObject(3, vec4(-1.3, 1.0, 0.3, 0.0), vec4(1.0, 2.0, 1.0, 0.0), vec3(1.0)),
		SceneObject(3, vec4(0.77, 0.75, -0.5, 0.0), vec4(1.5, 1.5, 1.5, 0.0), vec3(1.0))

		// ground
		, SceneObject(1, vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0), vec3(0.664, 0.824, 0.85))
#elif SCENE == 3
		SceneObject(3, vec4(0.0, 1.5, 2.6, 0.0), vec4(5.0, 3.0, 0.5, 0.0), vec3(1.0)),
		SceneObject(3, vec4(3.0, 0.75, -1.3, 0.0), vec4(1.5, 1.5, 1.5, 0.0), vec3(1.0)),

		SceneObject(4, vec4(0.25, 1.0, 1.5, 0.75), vec4(0.0, 1.0, 0.0, 2.0), vec3(1.0)),
		SceneObject(4, vec4(0.75, 0.2, 0.0, 0.2), vec4(1.0, 0.0, 0.0, 2.0), vec3(1.0)),
		SceneObject(4, vec4(0.75, 0.2, -0.4, 0.2), vec4(1.0, 0.0, 0.0, 2.0), vec3(1.0)),
		SceneObject(4, vec4(0.75, 0.2, -0.8, 0.2), vec4(1.0, 0.0, 0.0, 2.0), vec3(1.0))

		// ground
		, SceneObject(1, vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0), vec3(0.664, 0.824, 0.85))
#endif	// SCENE == 1
	);

	vec3 spos	= gl_FragCoord.xyz;
	vec4 ndc	= vec4(cpos.xy, 0.1, 1.0);
	vec4 wpos	= matViewProjInv * ndc;
	vec3 raydir;

	wpos /= wpos.w;
	raydir = normalize(wpos.xyz - eyePos);

	vec3 prev = texelFetch(prevIteration, ivec2(spos.xy), 0).rgb;
	vec3 curr = TraceScene(eyePos, raydir, spos, objects);
	float d = 1.0 / currSample;

	my_FragColor0.rgb = mix(prev, curr, d);
	my_FragColor0.a = 1.0;
}
