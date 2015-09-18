
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/fpscamera.h"
#include "../common/simplecollision.h"

// - area light specular + energy conservation
// - model the roof

#define OBJECT_ID_ROOM		0
#define OBJECT_ID_SOFA		1
#define OBJECT_ID_BUDDHA1	2
#define OBJECT_ID_BUDDHA2	3
#define OBJECT_ID_BUDDHA3	4
#define OBJECT_ID_BUDDHA4	5
#define OBJECT_ID_CAR		6
#define OBJECT_ID_TABLE		7
#define OBJECT_ID_TV		8
#define OBJECT_ID_DRAGON1	9
#define OBJECT_ID_DRAGON2	10
#define OBJECT_ID_DRAGON3	11
#define OBJECT_ID_DRAGON4	12
#define OBJECT_ID_CUPBOARDS	13
#define OBJECT_ID_ROOFTOP	14
#define OBJECT_ID_LAMP		15

#define LIGHT_ID_SKY		0
#define LIGHT_ID_LOCAL1		1
#define LIGHT_ID_SPOT		2
#define LIGHT_ID_TUBE		3
#define LIGHT_ID_RECT		4

#define NUM_OBJECTS			16
#define NUM_LIGHTS			5

// helper macros
#define TITLE				"Shader sample 24: Physically based rendering"
#define SHADOWMAP_SIZE		1024
#define USE_MSAA			0 //1
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

#define SAFE_DELETE_TEXTURE(x) \
	if( x ) { \
		glDeleteTextures(1, &x); \
		x = 0; }
// END

#define MAKE_DWORD_PTR(a, b) \
	reinterpret_cast<void*>(((a << 16) & 0xffff0000) | b)
// END

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample classes
class PBRLight
{
public:
	enum LightType
	{
		Point = 0, // TODO:
		Spot = 1,
		EnvProbe = 2,
		Area = 3
	};

	PBRLight();
	~PBRLight();

	void SetupAsSpot(float lumflux, float pos[3], float dir[3], float radius, float inner, float outer);
	void SetupAsEnvProbe(GLuint difftex, GLuint spectex);
	void SetupAsArea(float lumflux, float pos[3], float orient[4], float width, float height, float radius);

	void SetShadowBox(const OpenGLAABox& lightbox);
	void GetViewProj(float out[16]) const;

	float GetSpotAngleScale() const;
	float GetSpotAngleOffset() const;

	inline OpenGLMesh* GetGeometry() const {
		return geometry;
	}

	inline OpenGLFramebuffer* GetShadowMap() const {
		return shadowmap;
	}

	inline const float* GetPosition() const {
		return position;
	}

	inline const float* GetDirection() const {
		return direction;
	}

	inline const float* GetDirectionRight() const {
		return dirright;
	}

	inline const float* GetDirectionUp() const {
		return dirup;
	}

	inline const float* GetOrientation() const {
		return orientation;
	}

	inline const float* GetClipPlanes() const {
		return clipplanes;
	}

	inline float GetLuminuousIntensity() const {
		return lumintensity;
	}

	inline float GetLuminance() const {
		return luminance;
	}

	inline float GetRadius() const {
		return ((invradius == FLT_MAX) ? 0 : 1.0f / invradius);
	}

	inline float GetInvRadius() const {
		return invradius;
	}

	inline GLuint GetDiffuseIrrad() const {
		return diffirrad;
	}

	inline GLuint GetSpecularIrrad() const {
		return specirrad;
	}

	inline LightType GetType() const {
		return type;
	}

private:
	OpenGLAABox			box;
	float				orientation[4];	// for area lights
	float				position[3];
	float				dirright[3];	// when area
	float				dirup[3];		// when area
	float				direction[3];	// when spot
	float				angles[2];		// when spot (inner, outer)
	float				clipplanes[2];	// when spot (near, far)
	float				lumintensity;
	float				luminance;
	float				invradius;
	OpenGLMesh*			geometry;
	OpenGLFramebuffer*	shadowmap;
	LightType			type;
	GLuint				diffirrad;		// when envprobe
	GLuint				specirrad;		// when envprobe
};

class PBRMaterial
{
public:
	std::wstring	Name;
	OpenGLColor		BaseColor;
	float			Roughness;
	float			Metalness;
	GLuint			Texture;
	bool			Transparent;

	PBRMaterial();
	PBRMaterial(const WCHAR* name, OpenGLColor color, float roughness, float metalness, GLuint tex, bool transparent = false);
};

class SceneObject
{
	typedef std::vector<PBRMaterial> MaterialList;

private:
	MaterialList	materials;
	OpenGLAABox		boundbox;
	RigidBody*		body;
	OpenGLMesh*		mesh;
	GLuint			numsubsets;
	float			scaling;
	bool			isclone;

public:
	bool Visible; // for VFC

	SceneObject(const SceneObject& other);
	SceneObject(const char* file, RigidBody::BodyType type, float scale = 1.0f);
	SceneObject(OpenGLMesh* external, RigidBody::BodyType type, float scale = 1.0f);
	~SceneObject();

	void RecalculateBoundingBox();
	void Draw(OpenGLEffect* effect, bool transparent);
	void DrawFast(OpenGLEffect* effect);

	inline const OpenGLAABox& GetBoundingBox() const {
		return boundbox;
	}

	inline const PBRMaterial& GetMaterial(size_t index) const {
		return materials[index];
	}

	inline void SetMaterial(size_t index, const PBRMaterial& mat) {
		materials[index] = mat;
	}

	inline OpenGLMesh* GetMesh() {
		return mesh;
	}

	inline RigidBody* GetBody() {
		return body;
	}
};

// sample variables
FPSCamera*			camera			= 0;
OpenGLEffect*		pbrlightprobe	= 0;
OpenGLEffect*		pbrspotlight	= 0;
OpenGLEffect*		pbrarealight	= 0;
OpenGLEffect*		tonemap			= 0;
OpenGLEffect*		variance		= 0;
OpenGLEffect*		skyeffect		= 0;
OpenGLEffect*		saoeffect		= 0;
OpenGLEffect*		simplecolor		= 0;
OpenGLEffect*		varianceshadow	= 0;
OpenGLEffect*		boxblur3x3		= 0;
OpenGLEffect*		bilateralblur	= 0;
OpenGLEffect*		basic2D			= 0;
OpenGLEffect*		avgluminitial	= 0;
OpenGLEffect*		avglumiterative	= 0;
OpenGLEffect*		avglumfinal		= 0;
OpenGLEffect*		adaptluminance	= 0;
OpenGLFramebuffer*	msaaframebuffer	= 0;
OpenGLFramebuffer*	framebuffer		= 0;
OpenGLFramebuffer*	blockers		= 0;
OpenGLFramebuffer*	rawsao			= 0;
OpenGLFramebuffer*	blurredsao		= 0;
OpenGLFramebuffer*	avgluminance	= 0;
OpenGLFramebuffer*	adaptedlumprev	= 0;
OpenGLFramebuffer*	adaptedlumcurr	= 0;
OpenGLScreenQuad*	screenquad		= 0;
OpenGLMesh*			skymesh			= 0;
OpenGLMesh*			debugbox		= 0;
OpenGLAABox			scenebox;

GLuint				asphalttex		= 0;
GLuint				stuccotex		= 0;
GLuint				carpettex		= 0;
GLuint				woodtex			= 0;
GLuint				tvtex			= 0;
GLuint				skytexture		= 0;
GLuint				skyirraddiff	= 0;
GLuint				skyirradspec	= 0;
GLuint				localprobe1spec	= 0;
GLuint				integratedbrdf	= 0;
GLuint				infotex			= 0;

CollisionWorld*		collisionworld	= 0;
RigidBody*			selectedbody	= 0;
SceneObject*		objects[NUM_OBJECTS];
PBRLight*			lights[NUM_LIGHTS];
bool				saoenabled		= true;
bool				phydebug		= false;

float				DEBUG_avglum	= 0.1f;
float				DEBUG_exposure	= 1;

//*************************************************************************************************************
//
// PBRLight impl
//
//*************************************************************************************************************

PBRLight::PBRLight()
{
	type			= Point;
	lumintensity	= 1;
	luminance		= 1;
	invradius		= 1;

	diffirrad		= 0;
	specirrad		= 0;
	shadowmap		= 0;
	geometry		= 0;

	GLQuaternionIdentity(orientation);

	GLVec3Set(position, 0, 0, 0);
	GLVec3Set(dirright, 0, 0, 0);
	GLVec3Set(dirup, 0, 1, 0);
	GLVec3Set(direction, 0, 1, 0);
	
	angles[0] = cosf(GLDegreesToRadians(30));
	angles[1] = cosf(GLDegreesToRadians(45));
}

PBRLight::~PBRLight()
{
	SAFE_DELETE(shadowmap);
	SAFE_DELETE(geometry);
}

void PBRLight::SetupAsSpot(float lumflux, float pos[3], float dir[3], float radius, float inner, float outer)
{
	type = Spot;

	GLVec3Assign(position, pos);
	GLVec3Assign(direction, dir);

	angles[0] = cosf(inner);
	angles[1] = cosf(outer);

	shadowmap = new OpenGLFramebuffer(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	shadowmap->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_G32R32F, GL_LINEAR);
	shadowmap->Validate();

	lumintensity = lumflux / (2 * GL_PI * (1 - cosf(outer)));
	luminance = lumflux / (2 * GL_PI * GL_PI * (1 - cosf(outer)) * 1e-4f); // assume 1 cm
	invradius = ((radius == 0) ? FLT_MAX : 1.0f / radius);

	GLCreateCapsule(0.001f, 0.02f, &geometry);
}

void PBRLight::SetupAsEnvProbe(GLuint difftex, GLuint spectex)
{
	type			= EnvProbe;
	diffirrad		= difftex;
	specirrad		= spectex;
	lumintensity	= 1;
}

void PBRLight::SetupAsArea(float lumflux, float pos[3], float orient[4], float width, float height, float radius)
{
	type = Area;

	GLVec3Assign(position, pos);
	GLVec4Assign(orientation, orient);

	GLVec3Set(dirright, 1, 0, 0);
	GLVec3Set(dirup, 0, 1, 0);

	GLVec3Rotate(dirright, dirright, orient);
	GLVec3Rotate(dirup, dirup, orient);

	invradius = ((radius == 0) ? FLT_MAX : 1.0f / radius);

	// TODO: other types
	if( width == 0 && height == 0 )
	{
		// sphere
	}
	else if( width != 0 && height != 0 )
	{
		// rectangle
		GLVec3Scale(dirright, dirright, width * 0.5f);
		GLVec3Scale(dirup, dirup, height * 0.5f);

		GLCreatePlane(width, height, 1, 1, &geometry);

		lumintensity = 0;
		luminance = lumflux / (GL_PI * width * height);
	}
	else
	{
		// tube
		float length = GLMax(width, height);

		GLVec3Scale(dirright, dirright, length * 0.5f);
		GLVec3Scale(dirup, dirup, radius);

		GLCreateCapsule(length, radius, &geometry);

		lumintensity = 0;
		luminance = lumflux / (2 * radius * GL_PI * GL_PI * (length + 2 * radius));
	}
}

void PBRLight::SetShadowBox(const OpenGLAABox& castersbox)
{
	box = castersbox;

	float look[3];
	float up[3] = { 0, 0, 0 };

	up[((fabs(direction[1]) > 0.99f) ? 2 : 1)] = 1.0;

	GLVec3Add(look, position, direction);
	GLFitToBox(clipplanes[0], clipplanes[1], position, look, box);
}

void PBRLight::GetViewProj(float out[16]) const
{
	if( type == Spot )
	{
		float view[16];
		float proj[16];
		float look[3];
		float up[3] = { 0, 0, 0 };

		up[((fabs(direction[1]) > 0.99f) ? 2 : 1)] = 1.0;

		GLVec3Add(look, position, direction);

		GLMatrixLookAtRH(view, position, look, up);
		GLMatrixPerspectiveRH(proj, acosf(angles[1]) * 2.0f, 1, clipplanes[0], clipplanes[1]);
		GLMatrixMultiply(out, view, proj);
	}

	// TODO: point
}

float PBRLight::GetSpotAngleScale() const
{
	return 1.0f / GLMax<float>(0.001f, angles[0] - angles[1]);
}

float PBRLight::GetSpotAngleOffset() const
{
	return -angles[1] * GetSpotAngleScale();
}

//*************************************************************************************************************
//
// PBRMaterial impl
//
//*************************************************************************************************************

PBRMaterial::PBRMaterial()
{
	BaseColor	= 0xffffffff;
	Roughness	= 1.0f;
	Metalness	= 0.0f;
	Texture		= 0;
	Transparent	= false;
}

PBRMaterial::PBRMaterial(const WCHAR* name, OpenGLColor color, float roughness, float metalness, GLuint tex, bool transparent)
{
	Name		= name;
	BaseColor	= color;
	Roughness	= roughness;
	Metalness	= metalness;
	Texture		= tex;
	Transparent	= transparent;
}

//*************************************************************************************************************
//
// SceneObject impl
//
//*************************************************************************************************************

SceneObject::SceneObject(const SceneObject& other)
{
	if( other.mesh && other.body )
	{
		float size[3];
		float center[3];

		const OpenGLAABox& box = other.mesh->GetBoundingBox();

		if( other.body->GetType() == RigidBody::Box && collisionworld )
		{
			box.GetSize(size);
			box.GetCenter(center);

			body = collisionworld->AddStaticBox(size[0] * other.scaling, size[1] * other.scaling, size[2] * other.scaling);

			GLVec3Scale(center, center, -other.scaling);
			body->SetPivot(center);
		}
		else
			body = 0;
	}

	mesh		= other.mesh;
	numsubsets	= other.numsubsets;
	scaling		= other.scaling;
	isclone		= true;

	materials.resize(numsubsets);
	RecalculateBoundingBox();

	Visible = true;
}

SceneObject::SceneObject(const char* file, RigidBody::BodyType type, float scale)
{
	bool success = GLCreateMeshFromQM(file, 0, &numsubsets, &mesh);

	if( success )
	{
		float size[3];
		float center[3];

		const OpenGLAABox& box = mesh->GetBoundingBox();

		if( type == RigidBody::Box && collisionworld )
		{
			box.GetSize(size);
			box.GetCenter(center);

			body = collisionworld->AddStaticBox(size[0] * scale, size[1] * scale, size[2] * scale);

			GLVec3Scale(center, center, -scale);
			body->SetPivot(center);
		}
		else
			body = 0;

		scaling = scale;
		isclone = false;

		materials.resize(numsubsets);
		RecalculateBoundingBox();
	}

	Visible = true;
}

SceneObject::SceneObject(OpenGLMesh* external, RigidBody::BodyType type, float scale)
{
	if( external )
	{
		float size[3];
		float center[3];

		const OpenGLAABox& box = external->GetBoundingBox();

		if( type == RigidBody::Box && collisionworld )
		{
			box.GetSize(size);
			box.GetCenter(center);

			body = collisionworld->AddStaticBox(size[0] * scale, size[1] * scale, size[2] * scale);

			GLVec3Scale(center, center, -scale);
			body->SetPivot(center);
		}
		else
			body = 0;

		mesh		= external;
		numsubsets	= external->GetNumSubsets();
		scaling		= scale;
		isclone		= false;

		materials.resize(numsubsets);
		RecalculateBoundingBox();
	}

	Visible = true;
}

SceneObject::~SceneObject()
{
	if( !isclone )
		SAFE_DELETE(mesh);
}

void SceneObject::RecalculateBoundingBox()
{
	float world[16];

	if( body )
	{
		body->GetTransformWithSize(world);

		GLVec3Set(boundbox.Min, -0.5f, -0.5f, -0.5f);
		GLVec3Set(boundbox.Max, 0.5f, 0.5f, 0.5f);

		boundbox.TransformAxisAligned(world);
	}
	else if( mesh )
	{
		boundbox = mesh->GetBoundingBox();

		GLVec3Scale(boundbox.Min, boundbox.Min, scaling);
		GLVec3Scale(boundbox.Max, boundbox.Max, scaling);
	}
}

void SceneObject::Draw(OpenGLEffect* effect, bool transparent)
{
	if( !mesh || !Visible )
		return;

	float world[16];
	float worldinv[16];

	GLMatrixScaling(world, scaling, scaling, scaling);
	GLMatrixScaling(worldinv, 1.0f / scaling, 1.0f / scaling, 1.0f / scaling);

	if( body )
	{
		GLMatrixMultiply(world, world, body->GetTransform());
		GLMatrixMultiply(worldinv, body->GetInverseTransform(), worldinv);
	}

	effect->SetMatrix("matWorld", world);
	effect->SetMatrix("matWorldInv", worldinv);

	for( GLuint i = 0; i < numsubsets; ++i )
	{
		const PBRMaterial& mat = materials[i];
		float matparams[] = { mat.Roughness, mat.Metalness, 0, 0 };

		if( mat.Transparent != transparent )
			continue;

		if( mat.Texture != 0 )
		{
			GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, mat.Texture);
			matparams[2] = 1;
		}

		effect->SetVector("baseColor", &mat.BaseColor.r);
		effect->SetVector("matParams", matparams);
		effect->CommitChanges();

		mesh->DrawSubset(i);
	}
}

void SceneObject::DrawFast(OpenGLEffect* effect)
{
	if( !mesh )
		return;

	float world[16];

	GLMatrixScaling(world, scaling, scaling, scaling);

	if( body )
		GLMatrixMultiply(world, world, body->GetTransform());

	effect->SetMatrix("matWorld", world);
	effect->CommitChanges();

	for( GLuint i = 0; i < numsubsets; ++i )
	{
		if( !materials[i].Transparent )
			mesh->DrawSubset(i);
	}
}

//*************************************************************************************************************
//
// Sample functions
//
//*************************************************************************************************************

void RenderLocalProbe();
void RenderShadowmap();
void RenderScene(float viewproj[16], float eye[3], bool transparent);
void RenderSky(float eye[3], float view[16], float proj[16]);
void ResetCamera();
void DebugDrawBody(RigidBody::BodyType type, float xform[16]);

static bool IsAffectedByLight(int object, int light)
{
	bool affected = false;

	if( light == LIGHT_ID_SKY )
	{
		affected |= (object == OBJECT_ID_ROOM  || object == OBJECT_ID_ROOFTOP ||
			(object > OBJECT_ID_SOFA && object < OBJECT_ID_TABLE));
	}

	if( light == LIGHT_ID_LOCAL1 )
	{
		affected |= (
			object == OBJECT_ID_LAMP ||
			object == OBJECT_ID_CUPBOARDS ||
			object == OBJECT_ID_TABLE ||
			object == OBJECT_ID_DRAGON3 ||
			object == OBJECT_ID_TV);
	}

	if( light == LIGHT_ID_SPOT )
		affected |= (object < OBJECT_ID_ROOFTOP);

	if( light == LIGHT_ID_TUBE )
		affected = (object == OBJECT_ID_ROOFTOP);

	if( light == LIGHT_ID_RECT )
		affected = (object == OBJECT_ID_ROOM || (object >= OBJECT_ID_DRAGON1 && object <= OBJECT_ID_DRAGON4));

	return affected;
}

static OpenGLAABox CalculateShadowBox(int light)
{
	OpenGLAABox shadowbox;

	for( int j = 0; j < NUM_OBJECTS; ++j )
	{
		if( IsAffectedByLight(j, light) )
		{
			const OpenGLAABox& box = objects[j]->GetMesh()->GetBoundingBox();

			shadowbox.Add(box.Min);
			shadowbox.Add(box.Max);
		}
	}

	return shadowbox;
}

static void UpdateMaterialInfo(const PBRMaterial& material)
{
	Gdiplus::Bitmap*			bitmap;
	Gdiplus::Graphics*			graphics;

	Gdiplus::Color				textfill(0xffffffff);

	Gdiplus::SolidBrush			brush1(Gdiplus::Color(0x96000000));
	Gdiplus::SolidBrush			brush2(Gdiplus::Color(0xaa000000));
	Gdiplus::SolidBrush			brush3(Gdiplus::Color(0x8c000000));
	Gdiplus::SolidBrush			brush4(Gdiplus::Color(0xffeeeeee));
	Gdiplus::SolidBrush			brush5(Gdiplus::Color(0xffaaaaaa));

	Gdiplus::PrivateFontCollection	privfonts;
	std::wstring					wstr(L"../media/fonts/AGENCYR.ttf");

	Gdiplus::FontFamily			family;
	Gdiplus::StringFormat		format;
	Gdiplus::GraphicsPath		path1;
	Gdiplus::GraphicsPath		path2;
	INT							found;
	int							length;
	int							start = 256 - 212;

	privfonts.AddFontFile(L"../media/fonts/AGENCYR.ttf");
	privfonts.GetFamilies(1, &family, &found);

	bitmap = new Gdiplus::Bitmap(512, 256, PixelFormat32bppARGB);
	graphics = new Gdiplus::Graphics(bitmap);

	graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics->SetPageUnit(Gdiplus::UnitPixel);

	// render info
	graphics->FillRectangle(&brush1, 0, start, 512, 212);
	graphics->FillRectangle(&brush2, 10, start + 10, 492, 48);
	graphics->FillRectangle(&brush3, 10, start + 68, 492, 134);

	path1.AddString(material.Name.c_str(), material.Name.length(), &family, 0, 44, Gdiplus::Point(10, start + 10), &format);

	path2.AddString(L"Base color", 10, &family, 0, 32, Gdiplus::Point(12, start + 68 + 5), &format);
	path2.AddString(L"Roughness", 9, &family, 0, 32, Gdiplus::Point(12, start + 112 + 5), &format);
	path2.AddString(L"Metalness", 9, &family, 0, 32, Gdiplus::Point(12, start + 156 + 5), &format);

	format.SetAlignment(Gdiplus::StringAlignmentFar);
	wstr.resize(32);

	if( material.Texture == 0 )
		length = swprintf(&wstr[0], 32, L"(%.3f, %.3f, %.3f)", material.BaseColor.r, material.BaseColor.g, material.BaseColor.b);
	else
		length = swprintf(&wstr[0], 10, L"<texture>");
	
	path1.AddString(wstr.c_str(), length, &family, 0, 32, Gdiplus::Point(480, start + 68 + 5), &format);

	length = swprintf(&wstr[0], 32, L"%.2f", material.Roughness);
	path1.AddString(wstr.c_str(), length, &family, 0, 32, Gdiplus::Point(480, start + 112 + 5), &format);

	length = swprintf(&wstr[0], 32, L"%.2f", material.Metalness);
	path1.AddString(wstr.c_str(), length, &family, 0, 32, Gdiplus::Point(480, start + 156 + 5), &format);

	graphics->FillPath(&brush4, &path1);
	graphics->FillPath(&brush5, &path2);

	// copy to texture
	Gdiplus::Rect rc(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
	Gdiplus::BitmapData data;

	memset(&data, 0, sizeof(Gdiplus::BitmapData));
	bitmap->LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

	glBindTexture(GL_TEXTURE_2D, infotex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 256, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data.Scan0);

	bitmap->UnlockBits(&data);

	delete graphics;
	delete bitmap;

}

bool InitScene()
{
	OpenGLAABox	box;
	float		orient[4];

	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	glClearDepth(1.0);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if( !GLCreateMeshFromQM("../media/meshes10/sky.qm", 0, 0, &skymesh) )
	{
		MYERROR("Could not load 'sky'");
		return false;
	}

	collisionworld = new CollisionWorld();

	// textures
	GLCreateTextureFromFile("../media/textures/asphalt2.jpg", true, &asphalttex);
	GLCreateTextureFromFile("../media/textures/stucco.jpg", true, &stuccotex);
	GLCreateTextureFromFile("../media/textures/carpet4.jpg", true, &carpettex);
	GLCreateTextureFromFile("../media/textures/wood1.jpg", true, &woodtex);
	GLCreateTextureFromFile("../media/textures/scarlett.jpg", true, &tvtex);

	GLCreateCubeTextureFromFile("../media/textures/uffizi.dds", &skytexture);
	GLCreateCubeTextureFromFile("../media/textures/uffizi_diff_irrad.dds", &skyirraddiff);
	GLCreateCubeTextureFromFile("../media/textures/uffizi_spec_irrad.dds", &skyirradspec);
	GLCreateCubeTextureFromFile("../media/textures/local1_spec_irrad.dds", &localprobe1spec);

	GLCreateTextureFromFile("../media/textures/brdf.dds", false, &integratedbrdf);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLCreateTexture(512, 256, 1, GLFMT_A8R8G8B8, &infotex);

	// room
	objects[OBJECT_ID_ROOM] = new SceneObject("../media/meshes/livingroom.qm", RigidBody::None);

	PBRMaterial Asphalt(L"Asphalt", OpenGLColor(0.1f, 0.1f, 0.1f, 1), 0.8f, 0, asphalttex);
	PBRMaterial Carpet(L"Carpet", OpenGLColor(0.1f, 0.1f, 0.1f, 1), 0.8f, 0, carpettex);
	PBRMaterial Stucco(L"Stucco", OpenGLColor(1, 1, 1, 1), 1.0f, 0, stuccotex);
	PBRMaterial Tapestry(L"Tapestry", OpenGLColor::sRGBToLinear(175, 149, 151), 0.8f, 0, 0);
	PBRMaterial Window(L"Window glass", OpenGLColor(0.03f, 0.03f, 0.03f, 0.5f), 0.0f, 0.0f, 0, true);
	PBRMaterial Black(L"Black", OpenGLColor(0.01f, 0.01f, 0.01f, 1.0f), 0.15f, 0.0f, 0);
	PBRMaterial Wood(L"Wood", OpenGLColor::sRGBToLinear(119, 77, 0), 0.1f, 0, woodtex);
	PBRMaterial Aluminium(L"Aluminium", OpenGLColor(0.913f, 0.922f, 0.924f, 1.0f), 0.5f, 1.0f, 0);
	PBRMaterial WhiteWall(L"White wall", OpenGLColor(1, 1, 1, 1), 1.0f, 0, 0);

	objects[OBJECT_ID_ROOM]->SetMaterial(0, Stucco);
	objects[OBJECT_ID_ROOM]->SetMaterial(1, Tapestry);
	objects[OBJECT_ID_ROOM]->SetMaterial(2, Window);
	objects[OBJECT_ID_ROOM]->SetMaterial(3, Black);		// stair rail
	objects[OBJECT_ID_ROOM]->SetMaterial(4, Wood);		// stair
	objects[OBJECT_ID_ROOM]->SetMaterial(5, Asphalt);	// asphalt
	objects[OBJECT_ID_ROOM]->SetMaterial(6, Carpet);	// carpet
	objects[OBJECT_ID_ROOM]->SetMaterial(7, Aluminium);	// step
	objects[OBJECT_ID_ROOM]->SetMaterial(8, WhiteWall);	// shelve

	RigidBody* body = 0;

	body = collisionworld->AddStaticBox(14.0f, 0.01f, 16.0f);
	body->SetUserData(MAKE_DWORD_PTR(0, 6));

	body = collisionworld->AddStaticBox(6.0f, 3.1f, 0.2f);
	body->SetPosition(-1.0f, 1.55f, -5.0f);
	body->SetUserData(MAKE_DWORD_PTR(0, 1));

	body = collisionworld->AddStaticBox(0.2f, 3.1f, 6.0f);
	body->SetPosition(-4.0f, 1.55f, -2.0f);
	body->SetUserData(MAKE_DWORD_PTR(0, 1));

	body = collisionworld->AddStaticBox(3.0f, 3.1f, 0.1f);
	body->SetPosition(-2.5f, 1.55f, 1.0f);
	body->SetUserData(MAKE_DWORD_PTR(0, 2));

	body = collisionworld->AddStaticBox(0.2f, 3.1f, 3.0f);
	body->SetPosition(2.0f, 1.55f, -3.5f);
	body->SetUserData(MAKE_DWORD_PTR(0, 1));

	body = collisionworld->AddStaticBox(3.0f, 0.1f, 6.0f);
	body->SetPosition(-2.5f, 3.15f, -2.0f);
	body->SetUserData(MAKE_DWORD_PTR(0, 0));

	body = collisionworld->AddStaticBox(3.0f, 0.1f, 3.0f);
	body->SetPosition(0.5f, 3.15f, -3.5f);
	body->SetUserData(MAKE_DWORD_PTR(0, 0));

	body = collisionworld->AddStaticBox(6.0f, 1.0f, 0.3f);
	body->SetPosition(-1, 0.5f, -4.81f);
	body->SetUserData(MAKE_DWORD_PTR(0, 0));

	const float padlength = 2.35f;
	const float padz = -3.825f;
	const float padtop = 3.2f;
	const float slopethickness = 0.1f;

	body = collisionworld->AddStaticBox(1.3f, 0.1f, padlength);
	body->SetPosition(-4.65f, 3.15f, padz);
	body->SetUserData(MAKE_DWORD_PTR(0, 0));

	float smalldiag = sqrtf(slopethickness * slopethickness * 0.5f);
	float slopelength = sqrtf(2 * padtop * padtop);
	float slopey = ((slopelength - slopethickness) * padtop * 0.5f) / slopelength;
	float slopez = padz + padlength * 0.5f + (padtop - slopey) - smalldiag;

	body = collisionworld->AddStaticBox(1.0f, slopelength, slopethickness);
	GLQuaternionRotationAxis(orient, 1, 0, 0, GL_PI / -4);

	body->SetPosition(-4.62f, slopey, slopez);
	body->SetOrientation(orient);
	body->SetUserData(MAKE_DWORD_PTR(0, 4));

	// sofas
	PBRMaterial BlackPlastic(L"Black plastic", OpenGLColor(0, 0, 0, 1), 0.1f, 0, 0);
	PBRMaterial BlackLeather(L"Black leather", OpenGLColor(6.2e-3f, 5.14e-3f, 7.08e-3f, 1), 0.55f, 0, 0);

	objects[OBJECT_ID_SOFA] = new SceneObject("../media/meshes/sofa2.qm", RigidBody::Box);

	objects[OBJECT_ID_SOFA]->SetMaterial(0, BlackLeather);
	objects[OBJECT_ID_SOFA]->SetMaterial(1, BlackPlastic);

	GLQuaternionRotationAxis(orient, 0, 1, 0, GL_HALF_PI);

	objects[OBJECT_ID_SOFA]->GetBody()->SetPosition(-1.65f, 0, -2.0f);
	objects[OBJECT_ID_SOFA]->GetBody()->SetOrientation(orient);

	// buddhas
	PBRMaterial Gold(L"Gold", OpenGLColor(1.022f, 0.782f, 0.344f, 1), 0.1f, 1.0f, 0);
	PBRMaterial Silver(L"Silver", OpenGLColor(0.972f, 0.96f, 0.915f, 1), 0.1f, 1.0f, 0);
	PBRMaterial Copper(L"Copper", OpenGLColor(0.955f, 0.638f, 0.538f, 1), 0.1f, 1.0f, 0);
	PBRMaterial Zinc(L"Zinc", OpenGLColor(0.664f, 0.824f, 0.85f, 1), 0.1f, 1.0f, 0);

	objects[OBJECT_ID_BUDDHA1] = new SceneObject("../media/meshes/happy1.qm", RigidBody::Box, 10);
	objects[OBJECT_ID_BUDDHA2] = new SceneObject(*objects[OBJECT_ID_BUDDHA1]);
	objects[OBJECT_ID_BUDDHA3] = new SceneObject(*objects[OBJECT_ID_BUDDHA1]);
	objects[OBJECT_ID_BUDDHA4] = new SceneObject(*objects[OBJECT_ID_BUDDHA1]);

	objects[OBJECT_ID_BUDDHA1]->SetMaterial(0, Gold);
	objects[OBJECT_ID_BUDDHA2]->SetMaterial(0, Silver);
	objects[OBJECT_ID_BUDDHA3]->SetMaterial(0, Copper);
	objects[OBJECT_ID_BUDDHA4]->SetMaterial(0, Zinc);

	GLQuaternionRotationAxis(orient, 0, 1, 0, GL_PI * 0.5f);

	objects[OBJECT_ID_BUDDHA1]->GetBody()->SetPosition(5.0f, -0.5f, -0.75f);
	objects[OBJECT_ID_BUDDHA1]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_BUDDHA2]->GetBody()->SetPosition(5.0f, -0.5f, 0.25f);
	objects[OBJECT_ID_BUDDHA2]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_BUDDHA3]->GetBody()->SetPosition(5.0f, -0.5f, 1.25f);
	objects[OBJECT_ID_BUDDHA3]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_BUDDHA4]->GetBody()->SetPosition(5.0f, -0.5f, 2.25f);
	objects[OBJECT_ID_BUDDHA4]->GetBody()->SetOrientation(orient);

	// car
	objects[OBJECT_ID_CAR] = new SceneObject("../media/meshes/zonda.qm", RigidBody::Box);

	PBRMaterial Paint1(L"Car paint", OpenGLColor(1.0f, 0.2f, 0.0f, 1), 0.1f, 0.5f, 0);
	PBRMaterial Paint2(L"Car paint black", OpenGLColor(0.0f, 0.0f, 0.0f, 1), 0.1f, 0.5f, 0);
	PBRMaterial Chromium(L"Chromium", OpenGLColor(0.549f, 0.556f, 0.554f, 1), 0.2f, 1.0f, 0);
	PBRMaterial Glass(L"Glass", OpenGLColor(0.03f, 0.03f, 0.03f, 0.25f), 0.05f, 0.0f, 0, true);
	PBRMaterial BlackPolymer(L"Black polymer", OpenGLColor(0, 0, 0, 1), 0.5f, 0.0f, 0);
	PBRMaterial GreyPolymer(L"Grey polymer", OpenGLColor(0.01f, 0.01f, 0.01f, 1), 1.0f, 0.0f, 0);
	PBRMaterial RedLeather(L"Red leather", OpenGLColor(0.05f, 5.14e-4f, 7.08e-4f, 1), 0.9f, 0, 0);
	PBRMaterial BlackRubber(L"Black rubber", OpenGLColor(6.2e-4f, 5.14e-4f, 7.08e-4f, 1), 0.65f, 0, 0);
	PBRMaterial Mirror(L"Mirror", OpenGLColor(0.972f, 0.96f, 0.915f, 1), 0.0f, 1.0f, 0);

	GLuint carsubsets[] = { 0, 1, 5, 8, 3, 6, 9, 7, 4, 2 };
	objects[OBJECT_ID_CAR]->GetMesh()->ReorderSubsets(carsubsets);

	objects[OBJECT_ID_CAR]->SetMaterial(0, Paint1);			// chassis
	objects[OBJECT_ID_CAR]->SetMaterial(1, Paint2);			// lower chassis
	objects[OBJECT_ID_CAR]->SetMaterial(2, RedLeather);		// interior
	objects[OBJECT_ID_CAR]->SetMaterial(3, BlackPolymer);	// black parts
	objects[OBJECT_ID_CAR]->SetMaterial(4, GreyPolymer);	// grey parts
	objects[OBJECT_ID_CAR]->SetMaterial(5, Chromium);		// wheels
	objects[OBJECT_ID_CAR]->SetMaterial(6, BlackRubber);	// tires
	objects[OBJECT_ID_CAR]->SetMaterial(7, Chromium);		// exhaust
	objects[OBJECT_ID_CAR]->SetMaterial(8, Mirror);			// mirrors
	objects[OBJECT_ID_CAR]->SetMaterial(9, Glass);			// windows

	GLQuaternionRotationAxis(orient, 0, 1, 0, -GL_PI / 6);

	objects[OBJECT_ID_CAR]->GetBody()->SetPosition(1, 0, 4.5f);
	objects[OBJECT_ID_CAR]->GetBody()->SetOrientation(orient);

	// table
	objects[OBJECT_ID_TABLE] = new SceneObject("../media/meshes/table.qm", RigidBody::Box);

	PBRMaterial DarkGreyPlastic(L"Dark grey plastic", OpenGLColor(6.2e-3f, 6.2e-3f, 6.2e-3f, 1), 0.55f, 0, 0);
	PBRMaterial WhitePlastic(L"White plastic", OpenGLColor(1, 1, 1, 1), 0.05f, 0.0f, 0);

	GLuint tablesubsets[] = { 5, 1, 2, 3, 4, 0 };
	objects[OBJECT_ID_TABLE]->GetMesh()->ReorderSubsets(tablesubsets);

	objects[OBJECT_ID_TABLE]->SetMaterial(0, WhitePlastic);		// upper table
	objects[OBJECT_ID_TABLE]->SetMaterial(1, WhitePlastic);		// leg1
	objects[OBJECT_ID_TABLE]->SetMaterial(2, BlackPlastic);		// plate2
	objects[OBJECT_ID_TABLE]->SetMaterial(3, DarkGreyPlastic);	// lower table
	objects[OBJECT_ID_TABLE]->SetMaterial(4, WhitePlastic);		// leg2
	objects[OBJECT_ID_TABLE]->SetMaterial(5, BlackPlastic);		// plate1

	GLQuaternionRotationAxis(orient, 0, 1, 0, GL_HALF_PI);

	objects[OBJECT_ID_TABLE]->GetBody()->SetPosition(-2.7f, 0, -1.25f);
	objects[OBJECT_ID_TABLE]->GetBody()->SetOrientation(orient);

	// tv
	PBRMaterial TV(L"TV", OpenGLColor(0, 0, 0, 1), 0.0f, 0, tvtex);

	objects[OBJECT_ID_TV] = new SceneObject("../media/meshes/plasmatv.qm", RigidBody::Box);

	objects[OBJECT_ID_TV]->SetMaterial(0, TV);
	objects[OBJECT_ID_TV]->SetMaterial(1, DarkGreyPlastic);

	GLQuaternionRotationAxis(orient, 0, 1, 0, -GL_HALF_PI);

	objects[OBJECT_ID_TV]->GetBody()->SetPosition(-3.87f, 1.4f, -2.15f);
	objects[OBJECT_ID_TV]->GetBody()->SetOrientation(orient);

	// cupboards
	PBRMaterial BrownCupboard(L"Brown cupboard", OpenGLColor::sRGBToLinear(190, 174, 149), 0, 0.0f, 0);
	PBRMaterial BlackCupboard(L"Black cupboard", OpenGLColor(0, 0, 0, 1), 0, 0.0f, 0);
	PBRMaterial WhiteCupboard(L"White cupboard", OpenGLColor(1, 1, 1, 1), 0, 0.0f, 0);
	PBRMaterial WhiteBoard(L"White board", OpenGLColor(1, 1, 1, 1), 1.0f, 0.0f, 0);

	objects[OBJECT_ID_CUPBOARDS] = new SceneObject("../media/meshes/cupboards.qm", RigidBody::Box);

	objects[OBJECT_ID_CUPBOARDS]->SetMaterial(0, BlackCupboard);	// black parts
	objects[OBJECT_ID_CUPBOARDS]->SetMaterial(1, WhiteBoard);		// board
	objects[OBJECT_ID_CUPBOARDS]->SetMaterial(2, BrownCupboard);	// brown parts
	objects[OBJECT_ID_CUPBOARDS]->SetMaterial(3, WhiteCupboard);	// white parts

	// dragons
	PBRMaterial BlueRubber(L"Blue rubber (MERL)", OpenGLColor(0.05f, 0.08f, 0.17f, 1), 0.65f, 0, 0);
	PBRMaterial RedPlastic(L"Red plastic (MERL)", OpenGLColor(0.26f, 0.05f, 0.01f, 1), 0.08f, 0, 0);
	PBRMaterial YellowPaint(L"Yellow paint (MERL)", OpenGLColor(0.32f, 0.22f, 0.05f, 1), 0.68f, 0, 0);

	objects[OBJECT_ID_DRAGON1] = new SceneObject("../media/meshes/dragon.qm", RigidBody::Box, 0.05f);
	objects[OBJECT_ID_DRAGON2] = new SceneObject(*objects[OBJECT_ID_DRAGON1]);
	objects[OBJECT_ID_DRAGON3] = new SceneObject(*objects[OBJECT_ID_DRAGON1]);
	objects[OBJECT_ID_DRAGON4] = new SceneObject(*objects[OBJECT_ID_DRAGON1]);

	objects[OBJECT_ID_DRAGON1]->SetMaterial(0, BlueRubber);
	objects[OBJECT_ID_DRAGON2]->SetMaterial(0, RedPlastic);
	objects[OBJECT_ID_DRAGON3]->SetMaterial(0, Chromium);
	objects[OBJECT_ID_DRAGON4]->SetMaterial(0, YellowPaint);
	
	GLQuaternionRotationAxis(orient, 0, 1, 0, GL_PI);

	objects[OBJECT_ID_DRAGON1]->GetBody()->SetPosition(-3.5f, 1.01f, -4.81f);
	objects[OBJECT_ID_DRAGON1]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_DRAGON2]->GetBody()->SetPosition(-2.5f, 1.01f, -4.81f);
	objects[OBJECT_ID_DRAGON2]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_DRAGON3]->GetBody()->SetPosition(-1.5f, 1.01f, -4.81f);
	objects[OBJECT_ID_DRAGON3]->GetBody()->SetOrientation(orient);

	objects[OBJECT_ID_DRAGON4]->GetBody()->SetPosition(-0.5f, 1.01f, -4.81f);
	objects[OBJECT_ID_DRAGON4]->GetBody()->SetOrientation(orient);

	// roof
	objects[OBJECT_ID_ROOFTOP] = new SceneObject("../media/meshes/rooftop.qm", RigidBody::None);

	objects[OBJECT_ID_ROOFTOP]->SetMaterial(0, Carpet);
	objects[OBJECT_ID_ROOFTOP]->SetMaterial(1, WhiteWall);

	// lamp
	PBRMaterial LampOutside(L"Lamp outside", OpenGLColor(0, 0, 0, 1), 0.5f, 0.0f, 0);
	PBRMaterial LampInside(L"Lamp inside", OpenGLColor(10, 10, 10, 1), 0.2f, 0.0f, 0);

	objects[OBJECT_ID_LAMP] = new SceneObject("../media/meshes/lamp.qm", RigidBody::None);

	objects[OBJECT_ID_LAMP]->SetMaterial(0, LampOutside);
	objects[OBJECT_ID_LAMP]->SetMaterial(1, LampInside);

	// scene bounding box
	for( int i = 0; i < NUM_OBJECTS; ++i )
	{
		objects[i]->RecalculateBoundingBox();
		box = objects[i]->GetBoundingBox();

		if( objects[i]->GetBody() )
			objects[i]->GetBody()->SetUserData(MAKE_DWORD_PTR(i, 0));

		scenebox.Add(box.Min);
		scenebox.Add(box.Max);
	}

	// create render targets
#if USE_MSAA
	msaaframebuffer = new OpenGLFramebuffer(screenwidth, screenheight);

	msaaframebuffer->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, 4);
	msaaframebuffer->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, GLFMT_D32F, 4);

	if( !msaaframebuffer->Validate() )
		return false;
#endif

	framebuffer = new OpenGLFramebuffer(screenwidth, screenheight);
	framebuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, GL_LINEAR);
	framebuffer->AttachTexture(GL_DEPTH_ATTACHMENT, GLFMT_D32F);
	
	if( !framebuffer->Validate() )
		return false;

	blockers = new OpenGLFramebuffer(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	blockers->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_G32R32F, GL_LINEAR);
	blockers->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, GLFMT_D24S8);

	if( !blockers->Validate() )
		return false;

	rawsao = new OpenGLFramebuffer(screenwidth, screenheight);
	rawsao->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A8R8G8B8, GL_LINEAR);

	if( !rawsao->Validate() )
		return false;

	blurredsao = new OpenGLFramebuffer(screenwidth, screenheight);
	blurredsao->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A8R8G8B8, GL_LINEAR);

	if( !blurredsao->Validate() )
		return false;

	avgluminance = new OpenGLFramebuffer(64, 64);
	avgluminance->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R16F, GL_NEAREST);

	glGenerateMipmap(GL_TEXTURE_2D);

	if( !avgluminance->Validate() )
		return false;

	adaptedlumprev = new OpenGLFramebuffer(1, 1);
	adaptedlumcurr = new OpenGLFramebuffer(1, 1);

	adaptedlumprev->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R16F, GL_NEAREST);
	adaptedlumcurr->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R16F, GL_NEAREST);

	if( !adaptedlumprev->Validate() )
		return false;

	if( !adaptedlumcurr->Validate() )
		return false;

	adaptedlumcurr->Set();
	{
		glClearColor(0.1f, 0.1f, 0.1f, 1);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	adaptedlumcurr->Unset();

	screenquad = new OpenGLScreenQuad();

	// setup lights
	lights[LIGHT_ID_SKY] = new PBRLight();
	lights[LIGHT_ID_LOCAL1] = new PBRLight();
	lights[LIGHT_ID_SPOT] = new PBRLight();
	lights[LIGHT_ID_TUBE] = new PBRLight();
	lights[LIGHT_ID_RECT] = new PBRLight();

	lights[LIGHT_ID_SKY]->SetupAsEnvProbe(skyirraddiff, skyirradspec);
	lights[LIGHT_ID_LOCAL1]->SetupAsEnvProbe(skyirraddiff, localprobe1spec);

	float lightpos[3] = { -1.0f, 3.0f, -2.0f };
	float lightdir[3] = { 0, -1, 0 };

	// ~40 W
	lights[LIGHT_ID_SPOT]->SetupAsSpot(623, lightpos, lightdir, 9.0f, GLDegreesToRadians(75), GLDegreesToRadians(80));
	lights[LIGHT_ID_SPOT]->SetShadowBox(CalculateShadowBox(LIGHT_ID_SPOT));

	// ~10 W
	GLVec3Set(lightpos, -2.5f, 3.7f, -2.0f);
	GLQuaternionRotationAxis(orient, 0, 1, 0, GL_PI / 2);

	lights[LIGHT_ID_TUBE]->SetupAsArea(600, lightpos, orient, 3, 0, 0.1f);

	// ~2 W
	GLVec3Set(lightpos, -2, 2.1f, -4.81f);
	GLQuaternionRotationAxis(orient, 1, 0, 0, GL_HALF_PI);

	lights[LIGHT_ID_RECT]->SetupAsArea(120, lightpos, orient, 4, 0.3f, 0);

	// effects
	if( !GLCreateEffectFromFile("../media/shadersGL/pbr_lightprobe.vert", 0, "../media/shadersGL/pbr_lightprobe.frag", &pbrlightprobe) )
	{
		MYERROR("Could not load PBR envprobe shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/pbr_spotlight.vert", 0, "../media/shadersGL/pbr_spotlight.frag", &pbrspotlight) )
	{
		MYERROR("Could not load PBR spotlight shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/pbr_arealight.vert", 0, "../media/shadersGL/pbr_arealight.frag", &pbrarealight) )
	{
		MYERROR("Could not load PBR area light shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/tonemap.frag", &tonemap) )
	{
		MYERROR("Could not load gamma correction shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/sky.vert", 0, "../media/shadersGL/sky.frag", &skyeffect) )
	{
		MYERROR("Could not load sky shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/color.vert", 0, "../media/shadersGL/color.frag", &simplecolor) )
	{
		MYERROR("Could not load color shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/shadowmap_variance.vert", 0, "../media/shadersGL/shadowmap_variance.frag", &varianceshadow) )
	{
		MYERROR("Could not load shadowmap shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/boxblur3x3.frag", &boxblur3x3) )
	{
		MYERROR("Could not load blur shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/sao.frag", &saoeffect) )
	{
		MYERROR("Could not load SAO shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/bilateralblur.frag", &bilateralblur) )
	{
		MYERROR("Could not load bilateral blur shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &basic2D) )
	{
		MYERROR("Could not load basic 2D shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/avglum.frag", &avgluminitial, "#define SAMPLE_MODE 0\r\n") )
	{
		MYERROR("Could not load luminance measure (0) shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/avglum.frag", &avglumiterative, "#define SAMPLE_MODE 1\r\n") )
	{
		MYERROR("Could not load luminance measure (1) shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/avglum.frag", &avglumfinal, "#define SAMPLE_MODE 2\r\n") )
	{
		MYERROR("Could not load luminance measure (2) shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/adaptlum.frag", &adaptluminance) )
	{
		MYERROR("Could not load luminance adaptation shader");
		return false;
	}

	pbrspotlight->SetInt("sampler0", 0);
	pbrspotlight->SetInt("sampler1", 1);
	pbrspotlight->SetInt("sampler2", 2);

	pbrarealight->SetInt("sampler0", 0);
	pbrarealight->SetInt("sampler1", 1);
	pbrarealight->SetInt("sampler2", 2);

	pbrlightprobe->SetInt("sampler0", 0);
	pbrlightprobe->SetInt("sampler1", 1);
	pbrlightprobe->SetInt("sampler2", 2);
	pbrlightprobe->SetInt("sampler3", 3);

	adaptluminance->SetInt("sampler0", 0);
	adaptluminance->SetInt("sampler1", 1);

	tonemap->SetInt("sampler0", 0);
	tonemap->SetInt("sampler1", 1);

	skyeffect->SetInt("sampler0", 0);
	saoeffect->SetInt("sampler0", 0);
	bilateralblur->SetInt("sampler0", 0);
	boxblur3x3->SetInt("sampler0", 0);
	basic2D->SetInt("sampler0", 0);

	float white[] = { 1, 1, 1, 1 };
	simplecolor->SetVector("color", white);

	camera = new FPSCamera(collisionworld);

	camera->Fov = GLDegreesToRadians(50);
	camera->Aspect = (float)screenwidth / screenheight;

	// other
	OpenGLVertexElement decl[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	if( !GLCreateMesh(8, 24, 0, decl, &debugbox) )
	{
		MYERROR("Could not create debug box");
		return false;
	}

	float (*vdata)[3] = 0;
	GLushort* idata = 0;
	
	debugbox->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	debugbox->LockIndexBuffer(0, 0, GLLOCK_DISCARD, (void**)&idata);

	GLVec3Set(vdata[0], -0.5f, -0.5f, -0.5f);
	GLVec3Set(vdata[1], -0.5f, -0.5f, 0.5f);
	GLVec3Set(vdata[2], -0.5f, 0.5f, -0.5f);
	GLVec3Set(vdata[3], -0.5f, 0.5f, 0.5f);
	GLVec3Set(vdata[4], 0.5f, -0.5f, -0.5f);
	GLVec3Set(vdata[5], 0.5f, -0.5f, 0.5f);
	GLVec3Set(vdata[6], 0.5f, 0.5f, -0.5f);
	GLVec3Set(vdata[7], 0.5f, 0.5f, 0.5f);

	idata[0] = 0;	idata[8] = 4;	idata[16] = 0;
	idata[1] = 1;	idata[9] = 5;	idata[17] = 4;
	idata[2] = 0;	idata[10] = 4;	idata[18] = 2;
	idata[3] = 2;	idata[11] = 6;	idata[19] = 6;
	idata[4] = 1;	idata[12] = 5;	idata[20] = 1;
	idata[5] = 3;	idata[13] = 7;	idata[21] = 5;
	idata[6] = 2;	idata[14] = 6;	idata[22] = 3;
	idata[7] = 3;	idata[15] = 7;	idata[23] = 7;

	debugbox->UnlockIndexBuffer();
	debugbox->UnlockVertexBuffer();
	debugbox->GetAttributeTable()[0].PrimitiveType = GLPT_LINELIST;

	ResetCamera();
	RenderShadowmap();
	//RenderLocalProbe();
	UpdateMaterialInfo(Gold);

	return true;
}

void UninitScene()
{
	for( int i = 0; i < NUM_OBJECTS; ++i )
		SAFE_DELETE(objects[i]);

	for( int i = 0; i < NUM_LIGHTS; ++i )
		SAFE_DELETE(lights[i]);

	SAFE_DELETE(msaaframebuffer);
	SAFE_DELETE(framebuffer);
	SAFE_DELETE(blockers);
	SAFE_DELETE(rawsao);
	SAFE_DELETE(blurredsao);
	SAFE_DELETE(avgluminance);
	SAFE_DELETE(adaptedlumprev);
	SAFE_DELETE(adaptedlumcurr);

	SAFE_DELETE(pbrlightprobe);
	SAFE_DELETE(pbrspotlight);
	SAFE_DELETE(pbrarealight);
	SAFE_DELETE(variance);
	SAFE_DELETE(tonemap);
	SAFE_DELETE(skyeffect);
	SAFE_DELETE(simplecolor);
	SAFE_DELETE(varianceshadow);
	SAFE_DELETE(boxblur3x3);
	SAFE_DELETE(bilateralblur);
	SAFE_DELETE(saoeffect);
	SAFE_DELETE(basic2D);
	SAFE_DELETE(avgluminitial);
	SAFE_DELETE(avglumiterative);
	SAFE_DELETE(avglumfinal);
	SAFE_DELETE(adaptluminance);

	SAFE_DELETE(camera);
	SAFE_DELETE(collisionworld);
	SAFE_DELETE(screenquad);
	SAFE_DELETE(skymesh);
	SAFE_DELETE(debugbox);

	SAFE_DELETE_TEXTURE(asphalttex);
	SAFE_DELETE_TEXTURE(stuccotex);
	SAFE_DELETE_TEXTURE(carpettex);
	SAFE_DELETE_TEXTURE(woodtex);
	SAFE_DELETE_TEXTURE(tvtex);
	SAFE_DELETE_TEXTURE(skytexture);
	SAFE_DELETE_TEXTURE(skyirraddiff);
	SAFE_DELETE_TEXTURE(skyirradspec);
	SAFE_DELETE_TEXTURE(localprobe1spec);
	SAFE_DELETE_TEXTURE(integratedbrdf);
	SAFE_DELETE_TEXTURE(infotex);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
	camera->Event_KeyDown(keycode);
}

void Event_KeyUp(unsigned char keycode)
{
	camera->Event_KeyUp(keycode);

	if( keycode == 0x31 )
		saoenabled = !saoenabled;
	else if( keycode == 0x32 )
		phydebug = !phydebug;
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	camera->Event_MouseMove(dx, dy);
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	camera->Event_MouseDown(button);
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	if( button & 2 )
	{
		float m1[16];
		float m2[16];
		float p1[4], p2[4];
		float dir[3];

		camera->GetViewMatrix(m1);
		camera->GetProjectionMatrix(m2);

		GLMatrixMultiply(m1, m1, m2);
		GLMatrixInverse(m2, m1);

		GLVec4Set(p1, ((float)x / screenwidth) * 2 - 1, 1 - ((float)y / screenheight) * 2, 0, 1);
		GLVec4Set(p2, p1[0], p1[1], 0.1f, 1);

		GLVec4Transform(p1, p1, m2);
		GLVec4Transform(p2, p2, m2);

		GLVec3Scale(p1, p1, 1.0f / p1[3]);
		GLVec3Scale(p2, p2, 1.0f / p2[3]);

		GLVec3Subtract(dir, p2, p1);
		GLVec3Normalize(dir, dir);

		selectedbody = collisionworld->RayIntersect(p1, dir);

		if( selectedbody )
		{
			unsigned int data = reinterpret_cast<unsigned int>(selectedbody->GetUserData());
			SceneObject* selectedobject = objects[(data >> 16) & 0x0000ffff];

			if( selectedobject )
				UpdateMaterialInfo(selectedobject->GetMaterial(data & 0x0000ffff));
		}
	}

	camera->Event_MouseUp(button);
}

void ResetCamera()
{
	//camera->SetEyePosition(-2.43f, 1.8f, 0.681f);
	//camera->SetOrientation(6.42f, 0.16f, 0);

	camera->SetEyePosition(1.5f, 1.8f, -0.225f);
	camera->SetOrientation(5.15f, 0.12f, 0);
}

void Update(float delta)
{
	float campos[3];
	float center[3];

	camera->Update(delta);

	camera->GetEyePosition(campos);
	scenebox.GetCenter(center);

	if( GLVec3Distance(campos, center) > scenebox.Radius() * 1.5f )
		ResetCamera();
}

void CullScene(float viewproj[16])
{
	float planes[6][4] =
	{
		{ 1, 0, 0, 1 },
		{ -1, 0, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, -1, 0, 1 },
		{ 0, 0, 1, 0 },
		{ 0, 0, -1, 1 },
	};

	for( int i = 0; i < 6; ++i )
	{
		GLVec4TransformTranspose(planes[i], viewproj, planes[i]);
		GLPlaneNormalize(planes[i], planes[i]);
	}

	float	center[3];
	float	halfsize[3];
	float	dist, maxdist;
	int		numculled = 0;
	bool	visible;

	for( int i = 0; i < NUM_OBJECTS; ++i )
	{
		const OpenGLAABox& box = objects[i]->GetBoundingBox();

		box.GetCenter(center);
		box.GetHalfSize(halfsize);

		visible = true;

		for( int j = 0; j < 6; ++j )
		{
			const float* plane = planes[j];

			dist = GLPlaneDistance(plane, center);
			maxdist = (fabs(plane[0] * halfsize[0]) + fabs(plane[1] * halfsize[1]) + fabs(plane[2] * halfsize[2]));

			if( dist < -maxdist )
			{
				// outside
				visible = false;
				++numculled;
				break;
			}
		}

		objects[i]->Visible = visible;
	}
}

void RenderLocalProbe()
{
#define LOCAL_PROBE_SIZE	256

	float viewproj[16];
	float proj[16];
	float views[6][16];
	float eye[3], look[3], up[3];

	GLVec3Set(eye, -2.7f, 1.0f, -1.25f);

	GLVec3Set(up, 0, -1, 0);
	GLVec3Set(look, eye[0] + 1, eye[1], eye[2]);
	GLMatrixLookAtRH(views[0], eye, look, up);

	GLVec3Set(look, eye[0] - 1, eye[1], eye[2]);
	GLMatrixLookAtRH(views[1], eye, look, up);

	GLVec3Set(look, eye[0], eye[1], eye[2] + 1);
	GLMatrixLookAtRH(views[4], eye, look, up);

	GLVec3Set(look, eye[0], eye[1], eye[2] - 1);
	GLMatrixLookAtRH(views[5], eye, look, up);

	GLVec3Set(up, 0, 0, 1);
	GLVec3Set(look, eye[0], eye[1] + 1, eye[2]);
	GLMatrixLookAtRH(views[2], eye, look, up);

	GLVec3Set(up, 0, 0, -1);
	GLVec3Set(look, eye[0], eye[1] - 1, eye[2]);
	GLMatrixLookAtRH(views[3], eye, look, up);

	GLMatrixPerspectiveRH(proj, GL_PI * 0.5f, 1.0f, 0.1f, 20.0f);

	OpenGLFramebuffer* probe = new OpenGLFramebuffer(LOCAL_PROBE_SIZE, LOCAL_PROBE_SIZE);

	probe->AttachCubeTexture(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, GL_LINEAR);
	probe->AttachRenderbuffer(GL_DEPTH_STENCIL_ATTACHMENT, GLFMT_D24S8);

	if( !probe->Validate() )
		return;

	glClearColor(0, 0, 0, 1);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	saoenabled = false;

	probe->Set();
	{
		for( int i = 0; i < 6; ++i )
		{
			if( i > 0 )
				probe->Reattach(GL_COLOR_ATTACHMENT0, i, 0);

			GLMatrixMultiply(viewproj, views[i], proj);

			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

			// z only pass
			simplecolor->SetMatrix("matViewProj", viewproj);

			simplecolor->Begin();
			{
				for( int i = 0; i < NUM_OBJECTS; ++i )
					objects[i]->DrawFast(simplecolor);
			}
			simplecolor->End();

			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);

			// render with lighting
			RenderScene(viewproj, eye, false);

			glDisable(GL_BLEND);
			RenderSky(eye, views[i], proj);

			glDepthMask(GL_TRUE);
		}
	}
	probe->Unset();

	GLSaveFP16CubemapToFile("../media/textures/local1.dds", probe->GetColorAttachment(0));
	saoenabled = true;

	SAFE_DELETE(probe);
}

void RenderShadowmap()
{
	const PBRLight& light = *lights[LIGHT_ID_SPOT];

	float lightviewproj[16];
	float texelsize[] = { 1.0f / SHADOWMAP_SIZE, 1.0f / SHADOWMAP_SIZE };

	light.GetViewProj(lightviewproj);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	blockers->Set();
	{
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		varianceshadow->SetMatrix("matViewProj", lightviewproj);
		varianceshadow->SetVector("clipPlanes", light.GetClipPlanes());

		varianceshadow->Begin();

		for( int j = 0; j < NUM_OBJECTS; ++j )
		{
			if( IsAffectedByLight(j, LIGHT_ID_SPOT) )
				objects[j]->DrawFast(varianceshadow);
		}

		varianceshadow->End();
	}
	blockers->Unset();

	boxblur3x3->SetMatrix("texelSize", texelsize);
	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, blockers->GetColorAttachment(0));

	light.GetShadowMap()->Set();
	{
		boxblur3x3->Begin();
		{
			screenquad->Draw();
		}
		boxblur3x3->End();
	}
	light.GetShadowMap()->Unset();
}

void RenderSAO(float proj[16])
{
	if( !saoenabled )
		return;

	float clipinfo[4] = {
		camera->Near * camera->Far,
		camera->Near - camera->Far,
		camera->Far,
		screenheight / (2.0f * tanf(camera->Fov * 0.5f))
	};

	float projinfo[4] = {
		-2.0f / (screenwidth * proj[0]),
		-2.0f / (screenheight * proj[5]),
		(1.0f - proj[8]) / proj[0],
		(1.0f + proj[9]) / proj[5]
	};

	rawsao->Set();
	{
		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, framebuffer->GetDepthAttachment());

		saoeffect->SetVector("clipInfo", clipinfo);
		saoeffect->SetVector("projInfo", projinfo);
		saoeffect->Begin();

		screenquad->Draw();
		saoeffect->End();
	}

	blurredsao->Set();
	{
		float axis[] = { 1, 0, 0, 0 };

		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, rawsao->GetColorAttachment(0));

		bilateralblur->SetVector("axis", axis);
		bilateralblur->Begin();

		screenquad->Draw();
		bilateralblur->End();
	}
}

void ApplySAO()
{
	if( !saoenabled )
		return;

	float axis[] = { 0, 1, 1, 0 };

	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, blurredsao->GetColorAttachment(0));

	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	glDisable(GL_DEPTH_TEST);

	bilateralblur->SetVector("axis", axis);
	bilateralblur->Begin();
	{
		screenquad->Draw();
	}
	bilateralblur->End();

	glEnable(GL_DEPTH_TEST);
}

void RenderScene(float viewproj[16], float eye[3], bool transparent)
{
	float			lightviewproj[16];
	OpenGLEffect*	effect = 0;
	int				firstdrawn = 0;
	
	if( transparent )
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	else
		glBlendFunc(GL_ONE, GL_ONE);

	for( int i = 0; i < NUM_LIGHTS; ++i )
	{
		const PBRLight& light = *lights[i];

		switch( light.GetType() )
		{
		case PBRLight::Spot:
			effect = pbrspotlight;

			light.GetViewProj(lightviewproj);

			effect->SetMatrix("lightViewProj", lightviewproj);
			effect->SetVector("lightPos", light.GetPosition());
			effect->SetVector("lightDir", light.GetDirection());
			effect->SetVector("clipPlanes", light.GetClipPlanes());

			effect->SetFloat("lumIntensity", light.GetLuminuousIntensity());
			effect->SetFloat("angleScale", light.GetSpotAngleScale());
			effect->SetFloat("angleOffset", light.GetSpotAngleOffset());
			effect->SetFloat("invRadius", light.GetInvRadius());

			GLSetTexture(GL_TEXTURE2, GL_TEXTURE_2D, light.GetShadowMap()->GetColorAttachment(0));
			break;

		case PBRLight::EnvProbe:
			effect = pbrlightprobe;

			GLSetTexture(GL_TEXTURE1, GL_TEXTURE_CUBE_MAP, light.GetDiffuseIrrad());
			GLSetTexture(GL_TEXTURE2, GL_TEXTURE_CUBE_MAP, light.GetSpecularIrrad());
			GLSetTexture(GL_TEXTURE3, GL_TEXTURE_2D, integratedbrdf);
			break;

		case PBRLight::Area:
			effect = pbrarealight;

			effect->SetVector("lightPos", light.GetPosition());
			effect->SetVector("lightRight", light.GetDirectionRight());
			effect->SetVector("lightUp", light.GetDirectionUp());

			effect->SetFloat("luminance", light.GetLuminance());
			effect->SetFloat("radius", light.GetRadius());
		break;

		default:
			continue;
		}

		effect->SetVector("eyePos", eye);
		effect->SetMatrix("matViewProj", viewproj);

		effect->Begin();
		{
			for( int j = 0; j < NUM_OBJECTS; ++j )
			{
				if( IsAffectedByLight(j, i) )
					objects[j]->Draw(effect, transparent);
			}
		}
		effect->End();

		if( i == LIGHT_ID_SKY )
		{
			if( !transparent )
				ApplySAO();

			// NOTE: incorrect; assumes that all transparents are affected by first light
			glBlendFunc(GL_ONE, GL_ONE);
		}
	}
}

void RenderLights()
{
	float world[16];
	float color[4];

	simplecolor->Begin();

	for( int i = 0; i < NUM_LIGHTS; ++i )
	{
		const PBRLight& light = *lights[i];

		switch( light.GetType() )
		{
		case PBRLight::Spot:
		case PBRLight::Area:
			GLMatrixRotationQuaternion(world, light.GetOrientation());

			world[12] = light.GetPosition()[0];
			world[13] = light.GetPosition()[1];
			world[14] = light.GetPosition()[2];

			GLVec4Set(color, light.GetLuminance(), light.GetLuminance(),light.GetLuminance(), 1);

			simplecolor->SetMatrix("matWorld", world);
			simplecolor->SetVector("color", color);
			break;

		default:
			continue;
		}

		simplecolor->CommitChanges();
		light.GetGeometry()->DrawSubset(0);
	}

	simplecolor->End();
}

void MeasureLuminance()
{
	float texelsize[] = { 1.0f / (float)screenwidth, 1.0f / (float)screenheight, 0, 0 };

	avgluminance->Set();
	{
		// 64x64
		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, framebuffer->GetColorAttachment(0));

		avgluminitial->SetVector("texelSize", texelsize);
		avgluminitial->Begin();
		{
			screenquad->Draw();
		}
		avgluminitial->End();

		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, avgluminance->GetColorAttachment(0));

		// 16x16
		avgluminance->Reattach(GL_COLOR_ATTACHMENT0, 2);
		glViewport(0, 0, 16, 16);

		avglumiterative->SetInt("prevLevel", 0);
		avglumiterative->Begin();
		{
			screenquad->Draw();
		}
		avglumiterative->End();

		// 4x4
		avgluminance->Reattach(GL_COLOR_ATTACHMENT0, 4);
		glViewport(0, 0, 4, 4);

		avglumiterative->SetInt("prevLevel", 2);
		avglumiterative->Begin();
		{
			screenquad->Draw();
		}
		avglumiterative->End();

		// 1x1
		avgluminance->Reattach(GL_COLOR_ATTACHMENT0, 6);
		glViewport(0, 0, 1, 1);

		avglumfinal->SetInt("prevLevel", 4);
		avglumfinal->Begin();
		{
			screenquad->Draw();
		}
		avglumfinal->End();

		avgluminance->Reattach(GL_COLOR_ATTACHMENT0, 0);
	}
	avgluminance->Unset();
}

void AdaptLuminance(float elapsedtime)
{
	std::swap(adaptedlumcurr, adaptedlumprev);

	adaptedlumcurr->Set();
	{
		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, adaptedlumprev->GetColorAttachment(0));
		GLSetTexture(GL_TEXTURE1, GL_TEXTURE_2D, avgluminance->GetColorAttachment(0));

		adaptluminance->SetFloat("elapsedTime", elapsedtime);
		adaptluminance->Begin();
		{
			screenquad->Draw();
		}
		adaptluminance->End();

#if 0 //def _DEBUG
		GLushort bits;

		glReadPixels(0, 0, 1, 1, GL_RED, GL_HALF_FLOAT, &bits);

		DEBUG_avglum = GLHalfToFloat(bits);
		DEBUG_exposure = 1.0f / (9.6f * GLHalfToFloat(bits));

		//std::cout << "avg lum: " << DEBUG_avglum << ", exposure: " << DEBUG_exposure << "\n";
#endif
	}
	adaptedlumcurr->Unset();

	glViewport(0, 0, screenwidth, screenheight);
}

void RenderSky(float eye[3], float view[16], float proj[16])
{
	float world[16];
	float tmp[16];

	// draw sky
	GLMatrixScaling(world, 20, 20, 20);

	world[12] = eye[0];
	world[13] = eye[1];
	world[14] = eye[2];

	memcpy(tmp, proj, 16 * sizeof(float));

	// project to depth ~ 1.0
	tmp[10] = -1.0f + 1e-4f;
	tmp[14] = 0;

	GLMatrixMultiply(tmp, view, tmp);

	skyeffect->SetVector("eyePos", eye);
	skyeffect->SetMatrix("matWorld", world);
	skyeffect->SetMatrix("matViewProj", tmp);
	skyeffect->SetFloat("gamma", 1.0f); // RGBA16F is linear

	skyeffect->Begin();
	{
		GLSetTexture(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, skytexture);
		skymesh->DrawSubset(0);
	}
	skyeffect->End();
}

void Render(float alpha, float elapsedtime)
{
#if USE_MSAA
	OpenGLFramebuffer* rendertarget = msaaframebuffer;
#else
	OpenGLFramebuffer* rendertarget = framebuffer;
#endif

	float world[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float eye[3];

	camera->Animate(alpha);
	camera->FitToBox(scenebox);

	camera->GetViewMatrix(view);
	camera->GetProjectionMatrix(proj);
	camera->GetEyePosition(eye);

	GLMatrixMultiply(viewproj, view, proj);

	CullScene(viewproj);

	rendertarget->Set();
	{
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);

		// STEP 1: z only pass
		simplecolor->SetMatrix("matViewProj", viewproj);

		simplecolor->Begin();
		{
			for( int i = 0; i < NUM_OBJECTS; ++i )
				objects[i]->DrawFast(simplecolor);
		}
		simplecolor->End();

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);
		glDisable(GL_DEPTH_TEST);
	}

#if USE_MSAA
	msaaframebuffer->Resolve(framebuffer, GL_DEPTH_BUFFER_BIT);
#endif

	// STEP 2: SAO
	RenderSAO(proj);

	// STEP 3: render opaque objects
	rendertarget->Set();
	{
		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		RenderScene(viewproj, eye, false);

		glDisable(GL_BLEND);

		RenderSky(eye, view, proj);
	}

	// STEP 5: render transparent objects
	rendertarget->Set();
	{
		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		RenderScene(viewproj, eye, true);

		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		
		RenderLights();

		glDisable(GL_DEPTH_TEST);
	}
	rendertarget->Unset();

#if USE_MSAA
	msaaframebuffer->Resolve(framebuffer, GL_COLOR_BUFFER_BIT);
	framebuffer->Unset();
#endif

	// STEP 6: get average luminance
	MeasureLuminance();
	AdaptLuminance(elapsedtime);

	// STEP 7: tone map
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, framebuffer->GetColorAttachment(0));
	GLSetTexture(GL_TEXTURE1, GL_TEXTURE_2D, adaptedlumcurr->GetColorAttachment(0));

	tonemap->Begin();
	{
		screenquad->Draw();
	}
	tonemap->End();

	// draw debug
	if( phydebug )
	{
		simplecolor->Begin();
		collisionworld->DEBUG_Visualize(&DebugDrawBody);
		simplecolor->End();
	}

	// draw material info
	if( selectedbody )
	{
		OpenGLAABox box;
		float center[4];

		selectedbody->GetTransformWithSize(world);

		GLVec4Set(center, world[12], world[13], world[14], 1.0f);
		GLVec4Transform(center, center, viewproj);

		if( center[3] > camera->Near )
		{
			center[0] = (center[0] / center[3] * 0.5f + 0.5f) * screenwidth;
			center[1] = (center[1] / center[3] * 0.5f + 0.5f) * screenheight;

			float xzplane[4] = { 0, 1, 0, -0.5f };
			GLMatrixReflect(world, xzplane);

			glViewport((GLsizei)center[0], (GLsizei)center[1], 512, 256);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, infotex);

			basic2D->SetMatrix("matTexture", world);
			basic2D->Begin();
			{
				screenquad->Draw();
			}
			basic2D->End();

			glViewport(0, 0, screenwidth, screenheight);
		}
	}

#ifdef _DEBUG
	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";
#endif

	SwapBuffers(hdc);
}

void DebugDrawBody(RigidBody::BodyType type, float xform[16])
{
	simplecolor->SetMatrix("matWorld", xform);
	simplecolor->CommitChanges();

	debugbox->DrawSubset(0);
}
