
#ifdef _WIN32
#	include <Windows.h>
#	include <GdiPlus.h>
#endif

#include <iostream>
#include "../common/gl4x.h"

// helper macros
#define TITLE				"Shader sample 51: MSAA depth blit test for macOS"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

// external variables
#ifdef _WIN32
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;
#else
#	define SetWindowText(...)

unsigned int screenwidth = 0;
unsigned int screenheight = 0;
#endif

extern std::string GetResource(const std::string& name);

// sample variables
OpenGLMesh*			mesh					= 0;
OpenGLEffect*		effect					= 0;
OpenGLEffect*		combineeffect			= 0;
OpenGLFramebuffer*	bottomlayerMSAA			= 0;
OpenGLFramebuffer*	feedbacklayerMSAA		= 0;
OpenGLFramebuffer*	bottomlayerResolve		= 0;
OpenGLFramebuffer*	feedbacklayerResolve	= 0;

// sample shaders
const char* drawscreenquadVS = {
	"#version 150\n"

	"out vec2 tex;\n"

	"void main() {\n"
	"	vec4 positions[4] = vec4[4](\n"
	"		vec4(-1.0, -1.0, 0.0, 1.0),\n"
	"		vec4(1.0, -1.0, 0.0, 1.0),\n"
	"		vec4(-1.0, 1.0, 0.0, 1.0),\n"
	"		vec4(1.0, 1.0, 0.0, 1.0));\n"

	"	vec2 texcoords[4] = vec2[4](\n"
	"		vec2(0.0, 0.0),\n"
	"		vec2(1.0, 0.0),\n"
	"		vec2(0.0, 1.0),\n"
	"		vec2(1.0, 1.0));"

	"	tex = texcoords[gl_VertexID];\n"
	"	gl_Position = positions[gl_VertexID];\n"
	"}\n"
};

const char* drawscreenquadFS = {
	"#version 150\n"

	"uniform sampler2D sampler0;\n"

	"in vec2 tex;\n"
	"out vec4 my_FragColor0;\n"

	"void main() {\n"
	"	my_FragColor0 = texture(sampler0, tex);\n"
	"}\n"
};

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);

	if( !GLCreateMeshFromQM(GetResource("../media/meshes/teapot.qm").c_str(), &mesh) ) {
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateEffectFromFile(GetResource("../media/shadersGL/blinnphong.vert").c_str(), 0, GetResource("../media/shadersGL/blinnphong.frag").c_str(), &effect) ) {
		MYERROR("Could not load 'blinnphong' effect");
		return false;
	}
	
	if( !GLCreateEffectFromMemory(drawscreenquadVS, 0, drawscreenquadFS, &combineeffect) ) {
		MYERROR("Could not load 'combine' effect");
		return false;
	}

	// create frame buffers
	bottomlayerMSAA = new OpenGLFramebuffer(screenwidth, screenheight);

	bottomlayerMSAA->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, GLFMT_A8B8G8R8, 4);
	bottomlayerMSAA->AttachRenderbuffer(GL_DEPTH_STENCIL_ATTACHMENT, GLFMT_D24S8, 4);

	bottomlayerMSAA->Validate();

	feedbacklayerMSAA = new OpenGLFramebuffer(screenwidth, screenheight);

	feedbacklayerMSAA->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, GLFMT_A8B8G8R8, 4);
	feedbacklayerMSAA->AttachRenderbuffer(GL_DEPTH_STENCIL_ATTACHMENT, GLFMT_D24S8, 4);

	feedbacklayerMSAA->Validate();

	// create resolve framebuffers
	bottomlayerResolve = new OpenGLFramebuffer(screenwidth, screenheight);
	feedbacklayerResolve = new OpenGLFramebuffer(screenwidth, screenheight);

	bottomlayerResolve->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A8B8G8R8, GL_NEAREST);
	feedbacklayerResolve->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A8B8G8R8, GL_NEAREST);

	bottomlayerResolve->Validate();
	feedbacklayerResolve->Validate();

	return true;
}

void UninitScene()
{
	SAFE_DELETE(mesh);
	SAFE_DELETE(effect);
	SAFE_DELETE(combineeffect);
	SAFE_DELETE(bottomlayerMSAA);
	SAFE_DELETE(feedbacklayerMSAA);
	SAFE_DELETE(bottomlayerResolve);
	SAFE_DELETE(feedbacklayerResolve);

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
}

void Event_MouseScroll(int x, int y, short dz)
{
}

void Event_MouseDown(int x, int y, unsigned char button)
{
}

void Event_MouseUp(int x, int y, unsigned char button)
{
}

void Update(float delta)
{
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	float lightpos[4]	= { 6, 3, 10, 1 };
	float eye[4]		= { 0, 0, 3, 1 };
	float look[3]		= { 0, 0, 0 };
	float up[3]			= { 0, 1, 0 };
	float color1[4]		= { 1, 1, 1, 1 };
	float color2[4]		= { 0, 1, 0, 1 };

	float view[16];
	float proj[16];
	float world[16];
	float viewproj[16];
	float tmp1[16];
	float tmp2[16];

	GLMatrixLookAtRH(view, eye, look, up);
	GLMatrixPerspectiveFovRH(proj, (60.0f * 3.14159f) / 180.f,  (float)screenwidth / (float)screenheight, 0.1f, 100.0f);
	
	GLMatrixMultiply(viewproj, view, proj);

	// calculate world matrix
	GLMatrixIdentity(tmp2);
	
	tmp2[12] = -0.108f;		// offset with bb center
	tmp2[13] = -0.7875f;	// offset with bb center

	GLMatrixRotationAxis(tmp1, fmodf(time * 20.0f, 360.0f) * (3.14152f / 180.0f), 1, 0, 0);
	GLMatrixMultiply(world, tmp2, tmp1);

	GLMatrixRotationAxis(tmp2, fmodf(time * 20.0f, 360.0f) * (3.14152f / 180.0f), 0, 1, 0);
	GLMatrixMultiply(world, world, tmp2);

	world[12] -= 0.75f;

	//time += elapsedtime;
	time = 1.7f;

	effect->SetMatrix("matWorld", world);
	effect->SetMatrix("matViewProj", viewproj);
	effect->SetVector("lightPos", lightpos);
	effect->SetVector("eyePos", eye);
	effect->SetVector("color", color1);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// render bottom layer
	bottomlayerMSAA->Set();
	{
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		effect->Begin();
		{
			mesh->DrawSubset(0);
		}
		effect->End();
	}
	bottomlayerMSAA->Unset();

	// copy depth buffer to feedback layer <-- macOS driver bug
	feedbacklayerMSAA->Set();
	{
		// clear depth so the bug will be more obvious
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	feedbacklayerMSAA->Unset();
	
	bottomlayerMSAA->Resolve(feedbacklayerMSAA, GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	// render feedback layer
	world[12] += 1.5f;

	effect->SetMatrix("matWorld", world);
	effect->SetVector("color", color2);

	feedbacklayerMSAA->Set();
	{
		glClear(GL_COLOR_BUFFER_BIT);

		effect->Begin();
		{
			mesh->DrawSubset(0);
		}
		effect->End();
	}
	feedbacklayerMSAA->Unset();

	// resolve and combine
	bottomlayerMSAA->Resolve(bottomlayerResolve, GL_COLOR_BUFFER_BIT);
	feedbacklayerMSAA->Resolve(feedbacklayerResolve, GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	combineeffect->Begin();
	{
		glBindTexture(GL_TEXTURE_2D, bottomlayerResolve->GetColorAttachment(0));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindTexture(GL_TEXTURE_2D, feedbacklayerResolve->GetColorAttachment(0));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	combineeffect->End();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

#ifdef _WIN32
	SwapBuffers(hdc);
#endif
}
