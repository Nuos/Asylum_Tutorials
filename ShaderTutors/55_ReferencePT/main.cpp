
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

#define MAX_SAMPLES			16384

// helper macros
#define TITLE				"Shader sample 55: Reference path tracer"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
OpenGLEffect*		pathtracer			= 0;
OpenGLEffect*		presenteffect		= 0;

OpenGLFramebuffer*	rendertargets[2]	= { 0, 0 };		// for reference
OpenGLScreenQuad*	screenquad			= 0;
GLuint				text1				= 0;

BasicCamera			orbitcamera;
int					currtarget			= 0;
int					currsample			= 0;
int					mousedown			= 0;
bool				drawtext			= true;

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	// setup opengl
	glClearColor(0, 0, 0, 1);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	// create render targets for reference
	rendertargets[0] = new OpenGLFramebuffer(screenwidth, screenheight);
	rendertargets[1] = new OpenGLFramebuffer(screenwidth, screenheight);

	rendertargets[0]->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A32B32G32R32F, GL_NEAREST);
	rendertargets[1]->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A32B32G32R32F, GL_NEAREST);

	GL_ASSERT(rendertargets[0]->Validate());
	GL_ASSERT(rendertargets[1]->Validate());

	// load shaders
	if( !GLCreateEffectFromFile("../media/shadersGL/pathtracer.vert", 0, "../media/shadersGL/pathtracer.frag", &pathtracer) )
	{
		MYERROR("Could not load 'path tracer' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &presenteffect) )
	{
		MYERROR("Could not load 'basic2D' shader");
		return false;
	}

	float id[16];
	GLMatrixIdentity(id);

	presenteffect->SetMatrix("matTexture", id);

	screenquad = new OpenGLScreenQuad();

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8B8G8R8, &text1);

	GLRenderText(
		"Use the mouse to rotate camera\nH - Toggle help text",
		text1, 512, 512);

	// setup camera
	orbitcamera.SetAspect((float)screenwidth / (float)screenheight);
	orbitcamera.SetFov(GL_PI / 4.0f);
	orbitcamera.SetPosition(0, 1.633f, 0);
	orbitcamera.SetOrientation(GLDegreesToRadians(135), GLDegreesToRadians(30), 0);
	orbitcamera.SetDistance(6);
	orbitcamera.SetClipPlanes(0.1f, 20);

	return true;
}

void UninitScene()
{
	SAFE_DELETE(pathtracer);
	SAFE_DELETE(presenteffect);

	SAFE_DELETE(rendertargets[0]);
	SAFE_DELETE(rendertargets[1]);
	SAFE_DELETE(screenquad);

	GL_SAFE_DELETE_TEXTURE(text1);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode == 0x48 ) {	// H
		drawtext = !drawtext;
	}
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if( mousedown == 1 ) {
		orbitcamera.OrbitRight(GLDegreesToRadians(dx));
		orbitcamera.OrbitUp(GLDegreesToRadians(dy));
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
	orbitcamera.Update(delta);
}

void RenderReferenceWithPathTracing(float viewprojinv[16], float eye[3], float time)
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_FRAMEBUFFER_SRGB);
	glActiveTexture(GL_TEXTURE0);

	if( !orbitcamera.IsAnimationFinished() || mousedown == 1 )
		currsample = 0;

	if( currsample == 0 ) {
		rendertargets[currtarget]->Set();
		glClear(GL_COLOR_BUFFER_BIT);
	}

	if( currsample < MAX_SAMPLES ) {
		currtarget = 1 - currtarget;
		++currsample;

		// path trace
		rendertargets[currtarget]->Set();
		{
			glClear(GL_COLOR_BUFFER_BIT);
			glBindTexture(GL_TEXTURE_2D, rendertargets[1 - currtarget]->GetColorAttachment(0));

			pathtracer->SetMatrix("matViewProjInv", viewprojinv);
			pathtracer->SetVector("eyePos", eye);
			pathtracer->SetFloat("time", time);
			pathtracer->SetFloat("currSample", (float)currsample);

			pathtracer->Begin();
			{
				screenquad->Draw();
			}
			pathtracer->End();
		}
		rendertargets[currtarget]->Unset();
	}

	// present
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, rendertargets[currtarget]->GetColorAttachment(0));

	presenteffect->Begin();
	{
		screenquad->Draw();
	}
	presenteffect->End();
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	float world[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float viewinv[16];
	float viewprojinv[16];
	float eye[3];
	float clip[4] = { 0, 0, 0, 0 };

	orbitcamera.Animate(alpha);
	orbitcamera.GetViewMatrix(view);
	orbitcamera.GetProjectionMatrix(proj);
	orbitcamera.GetEyePosition(eye);

	clip[0] = orbitcamera.GetNearPlane();
	clip[1] = orbitcamera.GetFarPlane();

	GLMatrixInverse(viewinv, view);
	GLMatrixMultiply(viewproj, view, proj);
	GLMatrixInverse(viewprojinv, viewproj);

	glViewport(0, 0, screenwidth, screenheight);

	RenderReferenceWithPathTracing(viewprojinv, eye, time);

	if( drawtext ) {
		// render text
		glViewport(5, screenheight - 517, 512, 512);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float xzplane[4] = { 0, 1, 0, -0.5f };
		GLMatrixReflect(world, xzplane);

		presenteffect->SetMatrix("matTexture", world);
		presenteffect->Begin();
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, text1);

			screenquad->Draw();
		}
		presenteffect->End();

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		GLMatrixIdentity(world);
		presenteffect->SetMatrix("matTexture", world);
	}

	time += elapsedtime;

#ifdef _DEBUG
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";
#endif

	SwapBuffers(hdc);
}
