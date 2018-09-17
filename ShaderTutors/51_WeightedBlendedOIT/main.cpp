
#include <Windows.h>
#include <iostream>

#include "../common/gl4x.h"

// helper macros
#define TITLE				"Shader sample 51: Weighted-blended order independent transparency"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample structures
struct SceneObject
{
	int type;			// 0 for box, 1 for dragon, 2 for buddha
	float position[3];
	float scale[3];
	float angle;
	OpenGLColor color;
};

// sample variables
OpenGLMesh*			box				= 0;
OpenGLMesh*			buddha			= 0;
OpenGLMesh*			dragon			= 0;
OpenGLMesh*			billboard		= 0;
OpenGLFramebuffer*	accumulator		= 0;
OpenGLEffect*		draweffect		= 0;
OpenGLEffect*		composeeffect	= 0;
OpenGLAABox			scenebox;

GLuint				whitetex		= 0;
GLuint				texture			= 0;
GLuint				screenquadvao	= 0;

short				mousedx			= 0;
short				mousedy			= 0;
short				mousedown		= 0;

array_state<float, 2> cameraangle;

SceneObject objects[] =
{
	{ 0, { 0, -0.35f, 0 },		{ 15, 0.5f, 15 },		0,							OpenGLColor(1, 1, 0, 0.75f) },

	{ 1, { -1, -0.1f, 2.5f },	{ 0.3f, 0.3f, 0.3f },	-GL_PI / 8,					OpenGLColor(1, 0, 1, 0.5f) },
	{ 1, { 2.5f, -0.1f, 0 },	{ 0.3f, 0.3f, 0.3f },	GL_PI / -2 + GL_PI / -6,	OpenGLColor(0, 1, 1, 0.5f) },
	{ 1, { -2, -0.1f, -2 },		{ 0.3f, 0.3f, 0.3f },	GL_PI / -4,					OpenGLColor(1, 0, 0, 0.5f) },

	{ 2, { 0, -1.15f, 0 },		{ 20, 20, 20 },			GL_PI,						OpenGLColor(0, 1, 0, 0.5f) },
};

const int numobjects = sizeof(objects) / sizeof(SceneObject);

static void APIENTRY ReportGLError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userdata)
{
	if( type >= GL_DEBUG_TYPE_ERROR && type <= GL_DEBUG_TYPE_PERFORMANCE )
	{
		if( source == GL_DEBUG_SOURCE_API )
			std::cout << "GL(" << severity << "): ";
		else if( source == GL_DEBUG_SOURCE_SHADER_COMPILER )
			std::cout << "GLSL(" << severity << "): ";
		else
			std::cout << "OTHER(" << severity << "): ";

		std::cout << id << ": " << message << "\n";
	}
}

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures(hdc);

	if( !Quadron::qGLExtensions::ARB_shader_storage_buffer_object )
		return false;

#ifdef _DEBUG
	if( Quadron::qGLExtensions::ARB_debug_output )
	{
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
		glDebugMessageCallback(ReportGLError, 0);
	}
#endif

	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);

	// load objects
	if( !GLCreateMeshFromQM("../media/meshes/cube.qm", &box) )
	{
		MYERROR("Could not load box");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes/dragon.qm", &dragon) )
	{
		MYERROR("Could not load dragon");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes/happy1.qm", &buddha) )
	{
		MYERROR("Could not load buddha");
		return false;
	}

	OpenGLVertexElement decl[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0, 12, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	if( !GLCreateMesh(4, 0, 0, decl, &billboard) )
		return false;

	OpenGLBillboardVertex* vdata = 0;

	billboard->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	{
		vdata[0].x = -0.5f;	vdata[1].x = 0.5f;	vdata[2].x = -0.5f;	vdata[3].x = 0.5f;
		vdata[0].y = 0.5f;	vdata[1].y = 0.5f;	vdata[2].y = -0.5f;	vdata[3].y = -0.5f;
		vdata[0].z = 0;		vdata[1].z = 0;		vdata[2].z = 0;		vdata[3].z = 0;

		vdata[0].u = 0;		vdata[1].u = 1;		vdata[2].u = 0;		vdata[3].u = 1;
		vdata[0].v = 0;		vdata[1].v = 0;		vdata[2].v = 1;		vdata[3].v = 1;
	}
	billboard->UnlockVertexBuffer();

	billboard->GetAttributeTable()[0].PrimitiveType = GL_TRIANGLE_STRIP;

	// load shaders
	if( !GLCreateEffectFromFile("../media/shadersGL/ambient.vert", 0, "../media/shadersGL/weighted_blended.frag", &draweffect) )
		return false;

	if( !GLCreateEffectFromFile("../media/shadersGL/screenquad.vert", 0, "../media/shadersGL/weighted_compose.frag", &composeeffect) )
		return false;

	float uvscale[] = { 1, 1, 0, 0 };

	draweffect->SetVector("uv", uvscale);
	draweffect->SetInt("sampler0", 0);

	composeeffect->SetInt("sumColors", 0);
	composeeffect->SetInt("sumWeights", 1);

	// other
	if( !GLCreateTextureFromFile("../media/textures/jessy.png", false, &texture) )
		return false;

	glGenTextures(1, &whitetex);
	glBindTexture(GL_TEXTURE_2D, whitetex);
	{
		unsigned int wondercolor = 0xffffffff;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, &wondercolor);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glGenVertexArrays(1, &screenquadvao);
	glBindVertexArray(screenquadvao);
	{
		// empty
	}
	glBindVertexArray(0);

	// create framebuffer
	accumulator = new OpenGLFramebuffer(screenwidth, screenheight);

	accumulator->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, GL_NEAREST);
	accumulator->AttachTexture(GL_COLOR_ATTACHMENT1, GLFMT_R16F, GL_NEAREST);

	if( !accumulator->Validate() )
		return false;

	// calculate scene bounding box
	OpenGLAABox tmpbox;
	float world[16];
	float tmp[16];

	GLMatrixIdentity(world);

	for( int i = 0; i < numobjects; ++i )
	{
		const SceneObject& obj = objects[i];

		// scaling * rotation * translation
		GLMatrixScaling(tmp, obj.scale[0], obj.scale[1], obj.scale[2]);
		GLMatrixRotationAxis(world, obj.angle, 0, 1, 0);
		GLMatrixMultiply(world, tmp, world);

		GLMatrixTranslation(tmp, obj.position[0], obj.position[1], obj.position[2]);
		GLMatrixMultiply(world, world, tmp);

		if( obj.type == 0 )
			tmpbox = box->GetBoundingBox();
		else if( obj.type == 1 )
			tmpbox = dragon->GetBoundingBox();
		else if( obj.type == 2 )
			tmpbox = buddha->GetBoundingBox();

		tmpbox.TransformAxisAligned(world);

		scenebox.Add(tmpbox.Min);
		scenebox.Add(tmpbox.Max);
	}

	float angles[2] = { 0.25f, -0.7f };
	cameraangle = angles;

	return true;
}

void UninitScene()
{
	SAFE_DELETE(box);
	SAFE_DELETE(dragon);
	SAFE_DELETE(buddha);
	SAFE_DELETE(billboard);
	SAFE_DELETE(accumulator);
	SAFE_DELETE(draweffect);
	SAFE_DELETE(composeeffect);

	GL_SAFE_DELETE_TEXTURE(whitetex);
	GL_SAFE_DELETE_TEXTURE(texture);

	glDeleteVertexArrays(1, &screenquadvao);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	mousedx += dx;
	mousedy += dy;
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
	cameraangle.prev[0] = cameraangle.curr[0];
	cameraangle.prev[1] = cameraangle.curr[1];

	if( mousedown == 1 )
	{
		cameraangle.curr[0] -= mousedx * 0.004f;
		cameraangle.curr[1] -= mousedy * 0.004f;
	}

	// clamp to [-pi, pi]
	if( cameraangle.curr[1] >= 1.5f )
		cameraangle.curr[1] = 1.5f;

	if( cameraangle.curr[1] <= -1.5f )
		cameraangle.curr[1] = -1.5f;
}

void Render(float alpha, float elapsedtime)
{
	float world[16];
	float tmp[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float eye[3]		= { 0, 0.3f, 8 };
	float look[3]		= { 0, 0.3f, 0 };
	float up[3]			= { 0, 1, 0 };
	float clipplanes[2];
	float orient[2];

	cameraangle.smooth(orient, alpha);

	GLMatrixRotationAxis(view, orient[1], 1, 0, 0);
	GLMatrixRotationAxis(tmp, orient[0], 0, 1, 0);
	GLMatrixMultiply(view, view, tmp);

	GLVec3Transform(eye, eye, view);

	GLFitToBox(clipplanes[0], clipplanes[1], eye, look, scenebox);
	GLMatrixPerspectiveFovRH(proj, GLDegreesToRadians(60),  (float)screenwidth / (float)screenheight, clipplanes[0], clipplanes[1]);

	GLMatrixLookAtRH(view, eye, look, up);
	GLMatrixMultiply(viewproj, view, proj);

	// sum
	accumulator->Set();
	{
		float black[] = { 0, 0, 0, 0 };
		float white[] = { 1, 1, 1, 1 };

		glClearBufferfv(GL_COLOR, 0, black);
		glClearBufferfv(GL_COLOR, 1, white);

		glDisable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunci(0, GL_ONE, GL_ONE);
		glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

		draweffect->SetMatrix("matViewProj", viewproj);
		draweffect->Begin();
		{
			glBindTexture(GL_TEXTURE_2D, whitetex);

			GLMatrixIdentity(world);

			for( int i = 0; i < numobjects; ++i )
			{
				const SceneObject& obj = objects[i];

				// scaling * rotation * translation
				GLMatrixScaling(tmp, obj.scale[0], obj.scale[1], obj.scale[2]);
				GLMatrixRotationAxis(world, obj.angle, 0, 1, 0);
				GLMatrixMultiply(world, tmp, world);

				GLMatrixTranslation(tmp, obj.position[0], obj.position[1], obj.position[2]);
				GLMatrixMultiply(world, world, tmp);

				draweffect->SetMatrix("matWorld", world);
				draweffect->SetVector("matAmbient", &obj.color.r);
				draweffect->CommitChanges();

				if( obj.type == 0 )
					box->DrawSubset(0);
				else if( obj.type == 1 )
					dragon->DrawSubset(0);
				else if( obj.type == 2 )
					buddha->DrawSubset(0);
			}

			/*
			// draw some billboards
			glBindTexture(GL_TEXTURE_2D, texture);
			glDisable(GL_CULL_FACE);

			GLMatrixScaling(world, 3, 3, 1);

			world[12] = 2;
			world[13] = 1.5f;
			world[14] = -3;

			draweffect->SetMatrix("matWorld", world);
			draweffect->SetVector("matAmbient", white);
			draweffect->CommitChanges();

			billboard->DrawSubset(0);

			glEnable(GL_CULL_FACE);
			*/
		}
		draweffect->End();
	}
	accumulator->Unset();

	// compose
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, accumulator->GetColorAttachment(0));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, accumulator->GetColorAttachment(1));

	glBlendFuncSeparate(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ZERO, GL_ONE);

	composeeffect->Begin();
	{
		glBindVertexArray(screenquadvao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	composeeffect->End();

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

#ifdef _DEBUG
	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";
#endif

	SwapBuffers(hdc);
	mousedx = mousedy = 0;
}
