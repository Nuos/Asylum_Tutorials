
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"
#include "../common/spectatorcamera.h"

// helper macros
#define TITLE				"Shader sample 51: Camera test"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
BasicCamera		basiccamera;
SpectatorCamera	spectatorcamera;
OpenGLMesh*		mesh			= 0;	// z (outside of screen)
OpenGLEffect*	effect			= 0;
int				activecamera	= 0;
bool			mousedown		= false;

const char* vscode =
{
	"#version 150\n"

	"in vec3 my_Position;\n"
	"uniform mat4 matWVP;\n"

	"void main()\n"
	"{\n"
	"	gl_Position = matWVP * vec4(my_Position, 1);\n"
	"}\n"
};

const char* pscode =
{
	"#version 150\n"

	"uniform vec4 color;\n"
	"out vec4 my_FragColor0;\n"

	"void main()\n"
	"{\n"
	"	my_FragColor0 = color;\n"
	"}\n"
};

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	// setup opengl
	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if( !GLCreateMeshFromQM("../media/meshes/cube.qm", &mesh) )
	{
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateEffectFromMemory(vscode, NULL, pscode, &effect))
	{
		MYERROR("Could not create effect");
		return false;
	}

	// setup cameras
	basiccamera.SetAspect((float)screenwidth / screenheight);
	basiccamera.SetFov(GLDegreesToRadians(80));
	basiccamera.SetClipPlanes(0.1f, 5.0f);
	basiccamera.SetDistance(1.5f);

	spectatorcamera.Aspect = (float)screenwidth / screenheight;
	spectatorcamera.Fov = GLDegreesToRadians(80);
	spectatorcamera.Near = 0.1f;
	spectatorcamera.Far = 5.0f;

	spectatorcamera.SetEyePosition(0, 0, 1.5f);
	spectatorcamera.SetOrientation(0, 0, 0);

	return true;
}

void UninitScene()
{
	delete mesh;
	delete effect;

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
	if( activecamera == 1 )
		spectatorcamera.Event_KeyDown(keycode);

	if( keycode == VK_SPACE )
		activecamera = 1 - activecamera;
}

void Event_KeyUp(unsigned char keycode)
{
	if( activecamera == 1 )
		spectatorcamera.Event_KeyUp(keycode);
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if( activecamera == 1 )
		spectatorcamera.Event_MouseMove(dx, dy);

	if (mousedown) {
		if( activecamera == 0 ) {
			basiccamera.OrbitRight(GLDegreesToRadians(dx));
			basiccamera.OrbitUp(GLDegreesToRadians(dy));
		}
	}
}

void Event_MouseScroll(int x, int y, short dz)
{
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = true;

	if( activecamera == 1 )
		spectatorcamera.Event_MouseDown(button);
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown = false;

	if( activecamera == 1 )
		spectatorcamera.Event_MouseUp(button);
}

void Update(float delta)
{
	basiccamera.Update(delta);
	spectatorcamera.Update(delta);
}

void Render(float alpha, float elapsedtime)
{
	float wvp[16];
	float world[16];
	float view[16];
	float proj[16];
	float color[4];

	if( activecamera == 0 ) {
		basiccamera.Animate(alpha);

		basiccamera.GetViewMatrix(view);
		basiccamera.GetProjectionMatrix(proj);
	} else if( activecamera == 1 ) {
		spectatorcamera.Animate(alpha);

		spectatorcamera.GetViewMatrix(view);
		spectatorcamera.GetProjectionMatrix(proj);
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	effect->Begin();
	{
		// X axis
		GLMatrixScaling(world, 1, 0.1f, 0.1f);
		world[12] = 0.5f;

		GLMatrixMultiply(wvp, world, view);
		GLMatrixMultiply(wvp, wvp, proj);
		GLVec4Set(color, 1, 0, 0, 1);

		effect->SetMatrix("matWVP", wvp);
		effect->SetVector("color", color);
		effect->CommitChanges();

		mesh->DrawSubset(0);

		// Y axis
		GLMatrixScaling(world, 0.1f, 1, 0.1f);
		world[13] = 0.5f;

		GLMatrixMultiply(wvp, world, view);
		GLMatrixMultiply(wvp, wvp, proj);
		GLVec4Set(color, 0, 1, 0, 1);

		effect->SetMatrix("matWVP", wvp);
		effect->SetVector("color", color);
		effect->CommitChanges();

		mesh->DrawSubset(0);

		// Z axis
		GLMatrixScaling(world, 0.1f, 0.1f, 1);
		world[14] = 0.5f;

		GLMatrixMultiply(wvp, world, view);
		GLMatrixMultiply(wvp, wvp, proj);
		GLVec4Set(color, 0, 0, 1, 1);

		effect->SetMatrix("matWVP", wvp);
		effect->SetVector("color", color);
		effect->CommitChanges();

		mesh->DrawSubset(0);
	}
	effect->End();

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

	SwapBuffers(hdc);
}
