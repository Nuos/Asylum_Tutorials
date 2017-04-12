
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

#define MAX_SAMPLES			16384	// for path tracer

// helper macros
#define TITLE				"Shader sample 54: AO path tracer"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
OpenGLEffect*		pathtracer			= 0;			// reference path tracer
OpenGLEffect*		refgtaoeffect		= 0;			// reference GTAO effect
OpenGLEffect*		refgbuffereffect	= 0;			// reference G-buffer effect (path tracer)
OpenGLEffect*		combineeffect		= 0;
OpenGLEffect*		presenteffect		= 0;

OpenGLFramebuffer*	rendertargets[2]	= { 0, 0 };		// for reference
OpenGLFramebuffer*	gbuffer				= 0;
OpenGLScreenQuad*	screenquad			= 0;
GLuint				text1				= 0;

BasicCamera			orbitcamera;
int					currtarget			= 0;
int					currsample			= 0;
int					mousedown			= 0;
bool				drawtext			= true;
bool				userefgtao			= true;			// use reference GTAO/path tracer

static void CalculateSphereTetrahedron(int n, float radius)
{
	// places spheres in a tetrahedron formation
	const float cos30 = 0.86602f;
	const float sin30 = 0.5f;
	const float tan30 = 0.57735f;
	const float sqrt3 = 1.73205f;
	const float tH = 1.63299f;

	float start[3];
	float x, y, z;

	start[0] = -n * radius * tan30;
	start[1] = radius;

	for( int ly = 0; ly < n; ++ly ) {
		start[2] = -(n - ly - 1) * radius;

		for( int lx = 0; lx < n - ly; ++lx ) {
			for( int lz = 0; lz < n - lx - ly; ++lz ) {
				x = start[0] + lx * sqrt3 * radius;
				y = 0.01f + start[1] + ly * tH * radius;
				z = start[2] + lz * 2 * radius;

				printf("SceneObject(2, vec4(%.3f, %.3f, %.3f, 0.5), vec3(1.0, 0.3, 0.1)),\n", x, y, z);
				//printf("<shape type=\"sphere\">\n\t<point name=\"center\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\"/>\n\t<float name=\"radius\" value=\"0.5\"/>\n</shape>\n\n", x, y, z);
			}

			start[2] += 2 * radius * sin30;
		}

		start[0] += radius * tan30;
	}
}

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	//CalculateSphereTetrahedron(4, 0.5f);

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

	GLint value = 0;
	float white[] = { 1, 1, 1, 1 };

	gbuffer = new OpenGLFramebuffer(screenwidth, screenheight);

	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_sA8R8G8B8, GL_NEAREST);		// color
	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT1, GLFMT_A16B16G16R16F, GL_NEAREST);	// normals
	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT2, GLFMT_R32F, GL_NEAREST);			// depth
	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT3, GLFMT_G16R16F, GL_NEAREST);		// velocity

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	gbuffer->AttachRenderbuffer(GL_DEPTH_STENCIL_ATTACHMENT, GLFMT_D24S8);
	GL_ASSERT(gbuffer->Validate());

	glBindFramebuffer(GL_FRAMEBUFFER, gbuffer->GetFramebuffer());
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &value);
		GL_ASSERT(value == GL_SRGB);

		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &value);
		GL_ASSERT(value == GL_LINEAR);

		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &value);
		GL_ASSERT(value == GL_LINEAR);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// load shaders
	if( !GLCreateEffectFromFile("../media/shadersGL/AOpathtracer.vert", 0, "../media/shadersGL/AOpathtracer.frag", &pathtracer) )
	{
		MYERROR("Could not load 'path tracer' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/AOpathtracer.vert", 0, "../media/shadersGL/AOpathtracer.frag", &refgbuffereffect, "#define RENDER_GBUFFER\n") )
	{
		MYERROR("Could not load 'gbuffer' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/gtao.frag", &refgtaoeffect, "#define NUM_DIRECTIONS 32\n#define NUM_STEPS 16\n") )
	{
		MYERROR("Could not load 'GTAO' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/gtaocombine.frag", &combineeffect) )
	{
		MYERROR("Could not load 'combine' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &presenteffect) )
	{
		MYERROR("Could not load 'gamma correct' shader");
		return false;
	}

	float id[16];
	GLMatrixIdentity(id);

	presenteffect->SetMatrix("matTexture", id);
	refgtaoeffect->SetMatrix("matTexture", id);

	combineeffect->SetInt("sampler0", 0);
	combineeffect->SetInt("sampler1", 1);

	refgtaoeffect->SetInt("gbufferDepth", 0);
	refgtaoeffect->SetInt("gbufferNormals", 1);
	refgtaoeffect->SetInt("noise", 2);

	screenquad = new OpenGLScreenQuad();

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8R8G8B8, &text1);

	GLRenderText(
		"Use the mouse to rotate camera\n\nP - Toggle path tracer\nH - Toggle help text",
		text1, 512, 512);

	// setup camera
	orbitcamera.SetAspect((float)screenwidth / (float)screenheight);
	orbitcamera.SetFov(GL_PI / 4.0f);
	orbitcamera.SetPosition(0, 1.633f, 0);
	orbitcamera.SetDistance(6);
	orbitcamera.SetClipPlanes(0.1f, 20);

	return true;
}

void UninitScene()
{
	SAFE_DELETE(pathtracer);
	SAFE_DELETE(presenteffect);
	SAFE_DELETE(refgbuffereffect);
	SAFE_DELETE(refgtaoeffect);
	SAFE_DELETE(combineeffect);

	SAFE_DELETE(gbuffer);
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
	} else if( keycode == 0x50 ) {	// P
		userefgtao = !userefgtao;
		currsample = 0;
	}
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if( mousedown == 1 ) {
		orbitcamera.OrbitRight(GLDegreesToRadians(dx));
		orbitcamera.OrbitUp(GLDegreesToRadians(dy));
	}
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

void RenderReferenceWithGTAO(float proj[16], float clip[4], float eye[3])
{
	float projinfo[4] = {
		2.0f / (screenwidth * proj[0]),
		2.0f / (screenheight * proj[5]),
		-1.0f / proj[0],
		-1.0f / proj[5]
	};

	float invres[4] = {
		1.0f / screenwidth,
		1.0f / screenheight,
		0, 0
	};

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_FRAMEBUFFER_SRGB);

	gbuffer->Set();
	{
		glClear(GL_COLOR_BUFFER_BIT);

		refgbuffereffect->Begin();
		{
			screenquad->Draw();
		}
		refgbuffereffect->End();
	}
	gbuffer->Unset();

	// render AO
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(2));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(1));

	clip[2] = 0.5f * (screenheight / (2.0f * tanf(orbitcamera.GetFov() * 0.5f)));

	refgtaoeffect->SetVector("eyePos", eye);
	refgtaoeffect->SetVector("projInfo", projinfo);
	refgtaoeffect->SetVector("clipInfo", clip);
	refgtaoeffect->SetVector("invRes", invres);

	rendertargets[0]->Set();
	{
		refgtaoeffect->Begin();
		{
			screenquad->Draw();
		}
		refgtaoeffect->End();
	}
	rendertargets[0]->Unset();

	glActiveTexture(GL_TEXTURE0);

	// present
	glEnable(GL_FRAMEBUFFER_SRGB);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, rendertargets[0]->GetColorAttachment(0));

	presenteffect->Begin();
	{
		screenquad->Draw();
	}
	presenteffect->End();
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

	if( userefgtao ) {
		// using the reference spheres with GTAO
		refgbuffereffect->SetMatrix("matView", view);
		refgbuffereffect->SetMatrix("matViewInv", viewinv);
		refgbuffereffect->SetMatrix("matViewProjInv", viewprojinv);
		refgbuffereffect->SetVector("eyePos", eye);
		refgbuffereffect->SetVector("clipPlanes", clip);

		RenderReferenceWithGTAO(proj, clip, eye);
	} else {
		// using the reference spheres with path tracing
		RenderReferenceWithPathTracing(viewprojinv, eye, time);
	}

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
