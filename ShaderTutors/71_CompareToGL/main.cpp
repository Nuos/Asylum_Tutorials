
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>
#include <sstream>
#include <vector>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

// helper macros
#define TITLE				"Shader sample 71: Compare this to Vulkan"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(x[0]))

#define OBJECT_GRID_SIZE	64		// nxn objects
#define TILE_GRID_SIZE		32		// kxk tiles
#define SPACING				0.4f
#define CAMERA_SPEED		0.05f

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

struct GlobalUniformData {
	float viewproj[16];
	float lightpos[4];
	float eyepos[4];
};

class SceneObjectPrototype
{
private:
	OpenGLEffect*	effect;
	OpenGLMesh*		mesh;

public:
	SceneObjectPrototype(const char* filename);
	~SceneObjectPrototype();

	void SetGlobalUniformData(const GlobalUniformData& data);
	void Draw(const float transform[16], const float color[4]);

	inline OpenGLMesh* GetMesh()				{ return mesh; }
	inline const OpenGLMesh* GetMesh() const	{ return mesh; }
};

class SceneObject
{
private:
	float					world[16];
	float					color[4];
	SceneObjectPrototype*	proto;
	bool					rendered;

public:
	SceneObject(SceneObjectPrototype* prototype);

	void Draw();
	void GetBoundingBox(OpenGLAABox& outbox) const;

	inline float* GetTransform()						{ return world; }
	inline void SetMaterial(const float material[4])	{ GLVec4Assign(color, material); }
	inline void SetRendered(bool value)					{ rendered = value; }
};

typedef std::vector<SceneObject*> ObjectArray;

class FakeBatch
{
private:
	ObjectArray	objects;
	OpenGLAABox	boundingbox;

public:
	FakeBatch();
	~FakeBatch();

	void AddObject(SceneObject* obj);
	void Draw();

	inline const OpenGLAABox& GetBoundingBox() const	{ return boundingbox; }
	inline const ObjectArray& GetObjects() const		{ return objects; }
};

typedef std::vector<FakeBatch*> BatchArray;

SceneObjectPrototype*	prototypes[3]	= { 0, 0, 0 };
ObjectArray				sceneobjects;
BatchArray				tiles;
BatchArray				visibletiles;
BasicCamera				debugcamera;
OpenGLScreenQuad*		screenquad		= 0;
OpenGLEffect*			basic2D			= 0;
GLuint					text			= 0;

float					totalwidth;
float					totaldepth;
float					totalheight;

short					mousedx;
short					mousedy;
short					mousedown;

int						framerate		= 0;
int						frametime		= 0;
bool					debugmode		= false;

void UpdateTiles(float* viewproj);

//*************************************************************************************************************
//
// SceneObjectPrototype impl
//
//*************************************************************************************************************

SceneObjectPrototype::SceneObjectPrototype(const char* filename)
{
	GL_ASSERT(GLCreateMeshFromQM(filename, &mesh));
	GL_ASSERT(GLCreateEffectFromFile("../media/shadersGL/blinnphong.vert", 0, "../media/shadersGL/blinnphong.frag", &effect));
}

SceneObjectPrototype::~SceneObjectPrototype()
{
	delete effect;
	delete mesh;
}

void SceneObjectPrototype::SetGlobalUniformData(const GlobalUniformData& data)
{
	effect->SetMatrix("matViewProj", data.viewproj);
	effect->SetVector("lightPos", data.lightpos);
	effect->SetVector("eyePos", data.eyepos);
}

void SceneObjectPrototype::Draw(const float transform[16], const float color[4])
{
	effect->SetMatrix("matWorld", transform);
	effect->SetVector("color", color);

	effect->Begin();
	{
		for( GLuint i = 0; i < mesh->GetNumSubsets(); ++i )
			mesh->DrawSubset(i);
	}
	effect->End();
}

//*************************************************************************************************************
//
// SceneObject impl
//
//*************************************************************************************************************

SceneObject::SceneObject(SceneObjectPrototype* prototype)
{
	GL_ASSERT(prototype);

	proto = prototype;
	rendered = false;

	GLMatrixIdentity(world);
	GLVec4Set(color, 1, 1, 1, 1);
}

void SceneObject::Draw()
{
	if( !rendered )
		proto->Draw(world, color);

	rendered = true;
}

void SceneObject::GetBoundingBox(OpenGLAABox& outbox) const
{
	outbox = proto->GetMesh()->GetBoundingBox();
	outbox.TransformAxisAligned(world);
}

//*************************************************************************************************************
//
// FakeBatch impl
//
//*************************************************************************************************************

FakeBatch::FakeBatch()
{
}

FakeBatch::~FakeBatch()
{
}

void FakeBatch::AddObject(SceneObject* obj)
{
	OpenGLAABox objbox;

	if( objects.size() >= objects.capacity() )
		objects.reserve(objects.capacity() + 16);

	objects.push_back(obj);
	obj->GetBoundingBox(objbox);

	boundingbox.Add(objbox.Min);
	boundingbox.Add(objbox.Max);
}

void FakeBatch::Draw()
{
	for( size_t i = 0; i < objects.size(); ++i )
		objects[i]->Draw();
}

//*************************************************************************************************************
//
// Sample impl
//
//*************************************************************************************************************

static void UpdateText()
{
	std::stringstream ss;

	ss << "Frame rate: " << framerate << " (" << frametime << " ms)\n";
	ss << "Visible tiles: " << visibletiles.size() << "\n";
	
	GLRenderText(ss.str(), text, 800, 256);
}

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures(hdc);

	if( Quadron::qGLExtensions::WGL_EXT_swap_control )
		wglSwapInterval(0);

	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	// create objects
	float colors[4][4];
	float objscales[3][16];
	float objtranslate[16];
	float objrotate[16];
	OpenGLAABox objboxes[3];

	GLVec4Set(colors[0], 1, 0, 0, 1);
	GLVec4Set(colors[1], 0, 1, 0, 1);
	GLVec4Set(colors[2], 0, 0, 1, 1);
	GLVec4Set(colors[3], 1, 0.5f, 0, 1);

	GLMatrixScaling(objscales[0], 1, 1, 1);
	//GLMatrixScaling(objscales[1], 0.01f, 0.01f, 0.01f);
	GLMatrixScaling(objscales[1], 0.5f, 0.5f, 0.5f);
	//GLMatrixScaling(objscales[2], 16.0f, 16.0f, 16.0f);
	//GLMatrixScaling(objscales[2], 4.0f, 4.0f, 4.0f);
	GLMatrixScaling(objscales[2], 0.55f, 0.55f, 0.55f);

	prototypes[0] = new SceneObjectPrototype("../media/meshes/teapot.qm");
	//prototypes[1] = new SceneObjectPrototype("../media/meshes/angel.qm");
	prototypes[1] = new SceneObjectPrototype("../media/meshes/reventon/reventon.qm");
	//prototypes[2] = new SceneObjectPrototype("../media/meshes/happy1.qm");
	//prototypes[2] = new SceneObjectPrototype("../media/meshes/knot.qm");
	prototypes[2] = new SceneObjectPrototype("../media/meshes/zonda.qm");

	objboxes[0] = prototypes[0]->GetMesh()->GetBoundingBox();
	objboxes[1] = prototypes[1]->GetMesh()->GetBoundingBox();
	objboxes[2] = prototypes[2]->GetMesh()->GetBoundingBox();

	objboxes[0].TransformAxisAligned(objscales[0]);
	objboxes[1].TransformAxisAligned(objscales[1]);
	objboxes[2].TransformAxisAligned(objscales[2]);

	srand(0);

	totalwidth = OBJECT_GRID_SIZE * 3.2168f + (OBJECT_GRID_SIZE - 1) * SPACING;
	totaldepth = OBJECT_GRID_SIZE * 2.0f + (OBJECT_GRID_SIZE - 1) * SPACING;
	totalheight = 5;

	for( size_t i = 0; i < OBJECT_GRID_SIZE; ++i )
	{
		for( size_t j = 0; j < OBJECT_GRID_SIZE; ++j )
		{
			int index = rand() % 3;
			float angle = GLDegreesToRadians((float)(rand() % 360));

			SceneObjectPrototype* proto = prototypes[index];

			sceneobjects.push_back(new SceneObject(proto));

			GLMatrixRotationAxis(objrotate, angle, 0, 1, 0);
			GLMatrixTranslation(objtranslate, i * (3.2168f + SPACING) - totalwidth * 0.5f, 0, j * (2.0f + SPACING) - totaldepth * 0.5f);

			objtranslate[13] = -objboxes[index].Min[1];	// so they start at 0

			GLMatrixMultiply(sceneobjects.back()->GetTransform(), objscales[index], objrotate);
			GLMatrixMultiply(sceneobjects.back()->GetTransform(), sceneobjects.back()->GetTransform(), objtranslate);

			sceneobjects.back()->SetMaterial(colors[(rand() % 4)]);
		}
	}

	// create tiles
	OpenGLAABox	tilebox;
	OpenGLAABox	objbox;
	float		tilewidth = totalwidth / TILE_GRID_SIZE;
	float		tiledepth = totaldepth / TILE_GRID_SIZE;

	tiles.resize(TILE_GRID_SIZE * TILE_GRID_SIZE, 0);

	for( size_t i = 0; i < TILE_GRID_SIZE; ++i )
	{
		for( size_t j = 0; j < TILE_GRID_SIZE; ++j )
		{
			GLVec3Set(tilebox.Min, totalwidth * -0.5f + j * tilewidth, 0, totaldepth * -0.5f + i * tiledepth);
			GLVec3Set(tilebox.Max, totalwidth * -0.5f + (j + 1) * tilewidth, totalheight, totaldepth * -0.5f + (i + 1) * tiledepth);

			FakeBatch* batch = (tiles[i * TILE_GRID_SIZE + j] = new FakeBatch());

			for( size_t k = 0; k < sceneobjects.size(); ++k )
			{
				sceneobjects[k]->GetBoundingBox(objbox);

				if( tilebox.Intersects(objbox) )
					batch->AddObject(sceneobjects[k]);
			}
		}
	}

	visibletiles.reserve(tiles.size());

	// setup debug camera
	OpenGLAABox worldbox;
	float center[3];

	GLVec3Set(worldbox.Min, totalwidth * -0.5f, totalheight * -0.5f, totaldepth * -0.5f);
	GLVec3Set(worldbox.Max, totalwidth * 0.5f, totalheight * 0.5f, totaldepth * 0.5f);

	worldbox.GetCenter(center);

	debugcamera.SetAspect((float)screenwidth / screenheight);
	debugcamera.SetFov(GL_PI / 3);
	debugcamera.SetPosition(center[0], center[1], center[2]);
	debugcamera.SetDistance(GLVec3Distance(worldbox.Min, worldbox.Max) * 0.75f);
	debugcamera.SetClipPlanes(0.1f, GLVec3Distance(worldbox.Min, worldbox.Max) * 2);
	debugcamera.OrbitRight(0);
	debugcamera.OrbitUp(GLDegreesToRadians(30));

	// debug text
	screenquad = new OpenGLScreenQuad();

	GLCreateTexture(800, 256, 1, GLFMT_A8B8G8R8, &text);
	UpdateText();

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &basic2D) )
	{
		MYERROR("Could not load basic 2D shader");
		return false;
	}

	return true;
}

void UpdateTiles(float* viewproj)
{
	float planes[6][4];

	for( size_t i = 0; i < sceneobjects.size(); ++i )
		sceneobjects[i]->SetRendered(false);

	GLFrustumPlanes(planes, viewproj);
	visibletiles.clear();

	for( size_t i = 0; i < tiles.size(); ++i )
	{
		const OpenGLAABox& tilebox = tiles[i]->GetBoundingBox();

		if( GLFrustumIntersect(planes, tilebox) > 0 )
			visibletiles.push_back(tiles[i]);
	}
}

void UninitScene()
{
	if( text )
		glDeleteTextures(1, &text);

	delete screenquad;
	delete basic2D;

	for( size_t i = 0; i < tiles.size(); ++i )
		delete tiles[i];

	tiles.clear();
	tiles.swap(BatchArray());

	visibletiles.clear();
	visibletiles.swap(BatchArray());

	for( size_t i = 0; i < sceneobjects.size(); ++i )
		delete sceneobjects[i];

	sceneobjects.clear();
	sceneobjects.swap(ObjectArray());

	for( size_t i = 0; i < ARRAY_SIZE(prototypes); ++i )
		delete prototypes[i];

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode == 0x44 )
		debugmode = !debugmode;
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if( mousedown == 1 && debugmode ) {
		debugcamera.OrbitRight(GLDegreesToRadians(dx) * 0.5f);
		debugcamera.OrbitUp(GLDegreesToRadians(dy) * 0.5f);
	}
}

void Event_MouseScroll(int x, int y, short dz)
{
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = 1;
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown = 0;
}

void Update(float delta)
{
	debugcamera.Update(delta);
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;
	static float lasttime = 0;

	if( lasttime + 1.0f < time )
	{
		frametime = (int)(elapsedtime * 1000.0f);
		framerate = (int)(1.0f / elapsedtime);

		UpdateText();
		lasttime = time;
	}

	GlobalUniformData	globalunis;
	float				eye[3];
	float				look[3];
	float				tang[3];
	float				up[3]	= { 0, 1, 0 };

	float				world[16];
	float				view[16];
	float				proj[16];

	// setup transforms
	float				halfw	= totalwidth * 0.45f;
	float				halfd	= totaldepth * 0.45f;
	float				t		= time * ((CAMERA_SPEED * 32.0f) / OBJECT_GRID_SIZE);

	eye[0] = halfw * sinf(t * 2);
	eye[1] = totalheight + 8.0f;
	eye[2] = -halfd * cosf(t * 3);

	tang[0] = halfw * cosf(t * 2) * 2;
	tang[2] = halfd * sinf(t * 3) * 3;
	tang[1] = sqrtf(tang[0] * tang[0] + tang[2] * tang[2]) * -tanf(GLDegreesToRadians(60));

	GLVec3Normalize(tang, tang);
	GLVec3Add(look, eye, tang);

	GLMatrixLookAtRH(view, eye, look, up);
	GLMatrixPerspectiveFovRH(proj, (60.0f * 3.14159f) / 180.f,  (float)screenwidth / (float)screenheight, 0.1f, 100.0f);
	GLMatrixMultiply(globalunis.viewproj, view, proj);

	// setup uniforms
	float lightpos[4] = { 1, 0.4f, 0.2f, 1 };
	float radius = sqrtf(totalwidth * totalwidth * 0.25f + totalheight * totalheight * 0.25f + totaldepth * totaldepth * 0.25f);

	GLVec3Normalize(lightpos, lightpos);
	GLVec3Scale(lightpos, lightpos, radius * 1.5f);

	GLVec4Assign(globalunis.lightpos, lightpos);
	GLVec4Set(globalunis.eyepos, eye[0], eye[1], eye[2], 1);

	// update visible tiles
	UpdateTiles(globalunis.viewproj);
	time += elapsedtime;

	if( debugmode )
	{
		debugcamera.Animate(alpha);
		debugcamera.GetViewMatrix(view);
		debugcamera.GetProjectionMatrix(proj);

		GLMatrixMultiply(globalunis.viewproj, view, proj);
	}

	for( size_t i = 0; i < ARRAY_SIZE(prototypes); ++i )
		prototypes[i]->SetGlobalUniformData(globalunis);

	// render
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	for( size_t i = 0; i < visibletiles.size(); ++i )
		visibletiles[i]->Draw();

	// render text
	glViewport(3, -3 + screenheight - 256, 800, 256);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float xzplane[4] = { 0, 1, 0, -0.5f };
	GLMatrixReflect(world, xzplane);

	basic2D->SetMatrix("matTexture", world);
	basic2D->SetInt("sampler0", 0);
	basic2D->Begin();
	{
		glBindTexture(GL_TEXTURE_2D, text);
		screenquad->Draw();
	}
	basic2D->End();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glViewport(0, 0, screenwidth, screenheight);

#ifdef _DEBUG
	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";
#endif

	SwapBuffers(hdc);
}
