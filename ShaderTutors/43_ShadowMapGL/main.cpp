//*************************************************************************************************************
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"

// helper macros
#define TITLE				"Shader sample 43: OpenGL shadow mapping"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }
#define SHADOWMAP_SIZE		512

#define SAFE_DELETE_TEXTURE(x) \
	if( x ) { \
		glDeleteTextures(1, &x); \
		x = 0; }
// END

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
OpenGLEffect*		skyeffect		= 0;
OpenGLEffect*		bloom			= 0;
OpenGLEffect*		shadowedlight	= 0;
OpenGLEffect*		varianceshadow	= 0;
OpenGLEffect*		boxblur3x3		= 0;

OpenGLFramebuffer*	framebuffer		= 0;
OpenGLFramebuffer*	bloomtarget			= 0;
OpenGLFramebuffer*	shadowmap		= 0;
OpenGLFramebuffer*	blurredshadow	= 0;
OpenGLMesh*			angel			= 0;
OpenGLMesh*			table			= 0;
OpenGLMesh*			skymesh			= 0;
OpenGLScreenQuad*	screenquad		= 0;
OpenGLAABox			scenebox;

GLuint				tabletex		= 0;
GLuint				skytex			= 0;
GLuint				angeltex		= 0;

short				mousedx			= 0;
short				mousedy			= 0;
short				mousedown		= 0;

array_state<float, 2> cameraangle;
array_state<float, 2> lightangle;

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	glClearDepth(1.0);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	if( !GLCreateMeshFromQM("../media/meshes10/sky.qm", &skymesh) )
	{
		MYERROR("Could not load 'sky'");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes10/box.qm", &table) )
	{
		MYERROR("Could not load 'sky'");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes/angel.qm", &angel) )
	{
		MYERROR("Could not load 'sky'");
		return false;
	}

	screenquad = new OpenGLScreenQuad();

	// textures
	const char* files[6] =
	{
		"../media/textures/cubemap1_posx.jpg",
		"../media/textures/cubemap1_negx.jpg",
		"../media/textures/cubemap1_posy.jpg",
		"../media/textures/cubemap1_negy.jpg",
		"../media/textures/cubemap1_posz.jpg",
		"../media/textures/cubemap1_negz.jpg"
	};

	GLCreateTextureFromFile("../media/textures/burloak.jpg", true, &tabletex);
	GLCreateCubeTextureFromFiles(files, true, &skytex);
	GLCreateTexture(4, 4, 1, GLFMT_A8R8G8B8, &angeltex);

	GLuint data[] =
	{
		0xff888888, 0xff888888, 0xff888888, 0xff888888,
		0xff888888, 0xff888888, 0xff888888, 0xff888888,
		0xff888888, 0xff888888, 0xff888888, 0xff888888,
		0xff888888, 0xff888888, 0xff888888, 0xff888888
	};

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// render targets
	framebuffer = new OpenGLFramebuffer(screenwidth, screenheight);
	framebuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, GL_LINEAR);
	framebuffer->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, GLFMT_D24S8);

	if( !framebuffer->Validate() )
		return false;

	bloomtarget = new OpenGLFramebuffer(screenwidth, screenheight);
	bloomtarget->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A16B16G16R16F, GL_LINEAR);

	if( !bloomtarget->Validate() )
		return false;

	shadowmap = new OpenGLFramebuffer(SHADOWMAP_SIZE, SHADOWMAP_SIZE);
	shadowmap->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_G32R32F, GL_LINEAR);

	float border[] = { 1, 1, 1, 1 };

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	shadowmap->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, GLFMT_D24S8);

	if( !shadowmap->Validate() )
		return false;

	blurredshadow = new OpenGLFramebuffer(SHADOWMAP_SIZE, SHADOWMAP_SIZE);
	blurredshadow->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_G32R32F, GL_LINEAR);

	if( !blurredshadow->Validate() )
		return false;

	// effects
	if( !GLCreateEffectFromFile("../media/shadersGL/sky.vert", 0, "../media/shadersGL/sky.frag", &skyeffect) )
	{
		MYERROR("Could not load sky shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/simplebloom.frag", &bloom) )
	{
		MYERROR("Could not load gamma correction shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/shadowmap_variance.vert", 0, "../media/shadersGL/shadowmap_variance.frag", &varianceshadow) )
	{
		MYERROR("Could not load shadowmap shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/blinnphong_variance.vert", 0, "../media/shadersGL/blinnphong_variance.frag", &shadowedlight) )
	{
		MYERROR("Could not load shadowed light shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/boxblur3x3.frag", &boxblur3x3) )
	{
		MYERROR("Could not load blur shader");
		return false;
	}

	skyeffect->SetInt("sampler0", 0);
	boxblur3x3->SetInt("sampler0", 0);
	shadowedlight->SetInt("sampler0", 0);
	shadowedlight->SetInt("sampler1", 1);
	bloom->SetInt("sampler0", 0);
	bloom->SetInt("sampler1", 1);

	float angles1[2] = { GLDegreesToRadians(-225), 0.45f }; //{ -0.25f, 0.7f };
	float angles2[2] = { GLDegreesToRadians(-135), 0.78f };

	cameraangle = angles1;
	lightangle = angles2;

	GLVec3Set(scenebox.Min, -5, -1, -5);
	GLVec3Set(scenebox.Max, 5, 2.4f, 5);

	return true;
}
//*************************************************************************************************************
void UninitScene()
{
	SAFE_DELETE(skymesh);
	SAFE_DELETE(angel);
	SAFE_DELETE(table);
	SAFE_DELETE(screenquad);

	SAFE_DELETE(skyeffect);
	SAFE_DELETE(bloom);
	SAFE_DELETE(varianceshadow);
	SAFE_DELETE(shadowedlight);
	SAFE_DELETE(boxblur3x3);

	SAFE_DELETE(framebuffer);
	SAFE_DELETE(bloomtarget);
	SAFE_DELETE(shadowmap);
	SAFE_DELETE(blurredshadow);

	SAFE_DELETE_TEXTURE(skytex);
	SAFE_DELETE_TEXTURE(tabletex);
	SAFE_DELETE_TEXTURE(angeltex);

	GLKillAnyRogueObject();
}
//*************************************************************************************************************
void Event_KeyDown(unsigned char keycode)
{
}
//*************************************************************************************************************
void Event_KeyUp(unsigned char keycode)
{
}
//*************************************************************************************************************
void Event_MouseMove(int x, int y, short dx, short dy)
{
	mousedx += dx;
	mousedy += dy;
}
//*************************************************************************************************************
void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = button;
}
//*************************************************************************************************************
void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown &= (~button);
}
//*************************************************************************************************************
void Update(float delta)
{
	cameraangle.prev[0] = cameraangle.curr[0];
	cameraangle.prev[1] = cameraangle.curr[1];

	lightangle.prev[0] = lightangle.curr[0];
	lightangle.prev[1] = lightangle.curr[1];

	if( mousedown == 1 )
	{
		cameraangle.curr[0] += mousedx * 0.004f;
		cameraangle.curr[1] += mousedy * 0.004f;
	}

	if( mousedown == 2 )
	{
		lightangle.curr[0] += mousedx * 0.004f;
		lightangle.curr[1] += mousedy * 0.004f;
	}

	if( cameraangle.curr[1] >= 1.5f )
		cameraangle.curr[1] = 1.5f;

	if( cameraangle.curr[1] <= -0.3f )
		cameraangle.curr[1] = -0.3f;

	if( lightangle.curr[1] >= 1.5f )
		lightangle.curr[1] = 1.5f;

	if( lightangle.curr[1] <= 0.4f )
		lightangle.curr[1] = 0.4f;
}
//*************************************************************************************************************
void Render(float alpha, float elapsedtime)
{
	float world[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float lightview[16];
	float lightproj[16];
	float lightvp[16];

	float lightcolor[4]	= { 1, 1, 1, 1 };
	float lightpos[4]	= { 0, 0, 3, 1 };
	float eye[3]		= { 0, 0, 1.7f };
	float look[3]		= { 0, 0.5f, 0 };
	float up[3]			= { 0, 1, 0 };
	float uv[4]			= { 3, 3, 0, 1 };

	float clipplanes[2];
	float orient[2];
	float lightorient[2];

	cameraangle.smooth(orient, alpha);
	lightangle.smooth(lightorient, alpha);

	// setup light
	GLMatrixRotationRollPitchYaw(lightview, 0, lightorient[1], lightorient[0]);
	GLVec3Transform(lightpos, lightpos, lightview);

	lightpos[1] += 0.5f;

	GLFitToBox(clipplanes[0], clipplanes[1], lightpos, look, scenebox);
	GLMatrixPerspectiveFovRH(lightproj, GL_HALF_PI, 1, clipplanes[0], clipplanes[1]);

	GLMatrixLookAtRH(lightview, lightpos, look, up);
	GLMatrixMultiply(lightvp, lightview, lightproj);

	// setup camera
	GLMatrixRotationRollPitchYaw(view, 0, orient[1], orient[0]);
	GLVec3Transform(eye, eye, view);
	
	eye[1] += 0.5f;

	GLMatrixLookAtRH(view, eye, look, up);
	GLMatrixPerspectiveFovRH(proj, GL_HALF_PI, (float)screenwidth / (float)screenheight, 0.1f, 30.0f);
	GLMatrixMultiply(viewproj, view, proj);

	// render shadow map
	shadowmap->Set();
	{
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		GLMatrixScaling(world, 10, 0.2f, 10);
		world[13] = -1;

		varianceshadow->SetMatrix("matViewProj", lightvp);
		varianceshadow->SetVector("clipPlanes", clipplanes);
		varianceshadow->SetMatrix("matWorld", world);

		varianceshadow->Begin();
		{
			table->DrawSubset(0);

			GLMatrixScaling(world, 0.006667f, 0.006667f, 0.006667f);
			world[13] = -0.9f;

			varianceshadow->SetMatrix("matWorld", world);
			varianceshadow->CommitChanges();

			angel->DrawSubset(0);
		}
		varianceshadow->End();
	}
	shadowmap->Unset();

	// blur shadowmap
	float texelsize[] = { 1.0f / SHADOWMAP_SIZE, 1.0f / SHADOWMAP_SIZE };

	glDepthMask(GL_FALSE);
	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, shadowmap->GetColorAttachment(0));

	boxblur3x3->SetVector("texelSize", texelsize);

	blurredshadow->Set();
	{
		boxblur3x3->Begin();
		{
			screenquad->Draw();
		}
		boxblur3x3->End();
	}
	blurredshadow->Unset();

	glDepthMask(GL_TRUE);

	// render scene
	float ambient[] = { 0.1f, 0.1f, 0.1f, 1 };
	float black[] = { 0, 0, 0, 1 };
	float white[] = { 1, 1, 1, 1 };

	framebuffer->Set();
	{
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		// sky
		GLMatrixScaling(world, 20, 20, 20);

		world[12] = eye[0];
		world[13] = eye[1];
		world[14] = eye[2];

		skyeffect->SetVector("eyePos", eye);
		skyeffect->SetMatrix("matWorld", world);
		skyeffect->SetMatrix("matViewProj", viewproj);

		skyeffect->Begin();
		{
			GLSetTexture(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, skytex);
			skymesh->DrawSubset(0);
		}
		skyeffect->End();

		// objects
		GLSetTexture(GL_TEXTURE1, GL_TEXTURE_2D, blurredshadow->GetColorAttachment(0));

		shadowedlight->SetMatrix("matViewProj", viewproj);
		shadowedlight->SetMatrix("lightViewProj", lightvp);
		shadowedlight->SetVector("eyePos", eye);
		shadowedlight->SetVector("lightPos", lightpos);
		shadowedlight->SetVector("lightColor", lightcolor);
		shadowedlight->SetVector("clipPlanes", clipplanes);
		shadowedlight->SetVector("uv", uv);
		shadowedlight->SetFloat("ambient", 0.1f);

		shadowedlight->Begin();
		{
			// table
			GLMatrixScaling(world, 10, 0.2f, 10);
			world[13] = -1;

			shadowedlight->SetMatrix("matWorld", world);
			shadowedlight->SetMatrix("matWorldInv", world);
			shadowedlight->SetVector("matSpecular", black);
			shadowedlight->CommitChanges();

			GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, tabletex);
			table->DrawSubset(0);

			// angel
			GLMatrixScaling(world, 0.006667f, 0.006667f, 0.006667f);
			world[13] = -0.9f;

			shadowedlight->SetMatrix("matWorld", world);
			shadowedlight->SetMatrix("matWorldInv", world);
			shadowedlight->SetVector("matSpecular", white);
			shadowedlight->CommitChanges();

			GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, angeltex);
			angel->DrawSubset(0);
		}
		shadowedlight->End();

		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
	framebuffer->Unset();

	// bloom it
	texelsize[0] = 1.0f / screenwidth;
	texelsize[1] = 1.0f / screenheight;

	glDepthMask(GL_FALSE);
	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, framebuffer->GetColorAttachment(0));

	boxblur3x3->SetVector("texelSize", texelsize);

	bloomtarget->Set();
	{
		boxblur3x3->Begin();
		{
			screenquad->Draw();
		}
		boxblur3x3->End();
	}
	bloomtarget->Unset();

	glDepthMask(GL_TRUE);

	// gamma correct
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	GLSetTexture(GL_TEXTURE0, GL_TEXTURE_2D, framebuffer->GetColorAttachment(0));
	GLSetTexture(GL_TEXTURE1, GL_TEXTURE_2D, bloomtarget->GetColorAttachment(0));

	bloom->Begin();
	{
		screenquad->Draw();
	}
	bloom->End();

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

	SwapBuffers(hdc);
	mousedx = mousedy = 0;
}
//*************************************************************************************************************
