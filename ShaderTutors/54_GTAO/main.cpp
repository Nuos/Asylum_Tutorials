
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"
#include "../common/spectatorcamera.h"

#define METERS_PER_UNIT		0.01f	// for Sponza

// helper macros
#define TITLE				"Shader sample 54: GTAO"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
OpenGLEffect*		gbuffereffect		= 0;			// model G-buffer effect
OpenGLEffect*		gtaoeffect			= 0;			// model GTAO effect (jittered)
OpenGLEffect*		blureffect			= 0;			// spatial denoiser
OpenGLEffect*		accumeffect			= 0;			// temporal denoiser
OpenGLEffect*		combineeffect		= 0;
OpenGLEffect*		presenteffect		= 0;

OpenGLFramebuffer*	gbuffer				= 0;
OpenGLFramebuffer*	gtaotarget			= 0;
OpenGLFramebuffer*	blurtarget			= 0;
OpenGLFramebuffer*	accumtargets[2]		= { 0, 0 };
OpenGLMesh*			model				= 0;
OpenGLScreenQuad*	screenquad			= 0;

GLuint				depthbuffers[2]		= { 0, 0 };
GLuint				noisetex			= 0;
GLuint				supplytex			= 0;
GLuint				supplynormalmap		= 0;
GLuint				text1				= 0;

// matrices needed for temporal reprojection
float				prevV[16];
float				currV[16];

SpectatorCamera		camera;
float				frametime			= 0;
int					currtarget			= 0;
int					currsample			= 0;
bool				drawtext			= true;
bool				useblur				= true;
int					gtaomode			= 3;

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

	// load model
	GL_ASSERT(GLCreateMeshFromQM("../media/meshes/sponza/sponza.qm", &model));

	std::cout << "Generating tangent frame...\n";

	model->GenerateTangentFrame();
	model->EnableSubset(4, GL_FALSE);
	model->EnableSubset(259, GL_FALSE);

	// create substitute textures
	GLuint normal = 0xffff7f7f;	// (1, 0.5f, 0.5f)
	OpenGLMaterial* materials = model->GetMaterialTable();

	GLCreateTextureFromFile("../media/textures/vk_logo.jpg", true, &supplytex);
	GLCreateTexture(1, 1, 1, GLFMT_A8R8G8B8, &supplynormalmap, &normal);

	for( GLuint i = 0; i < model->GetNumSubsets(); ++i )
	{
		if( materials[i].Texture == 0 )
			materials[i].Texture = supplytex;

		if( materials[i].NormalMap == 0 )
			materials[i].NormalMap = supplynormalmap;
	}

	// create render targets
	GLint value = 0;
	float white[] = { 1, 1, 1, 1 };

	gbuffer = new OpenGLFramebuffer(screenwidth, screenheight);

	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_sA8R8G8B8, GL_NEAREST);		// color
	gbuffer->AttachTexture(GL_COLOR_ATTACHMENT1, GLFMT_A16B16G16R16F, GL_NEAREST);	// normals
	
	// depth buffers
	GLCreateTexture(screenwidth, screenheight, 1, GLFMT_R32F, &depthbuffers[0]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	GLCreateTexture(screenwidth, screenheight, 1, GLFMT_R32F, &depthbuffers[1]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	gbuffer->Attach(GL_COLOR_ATTACHMENT2, depthbuffers[0], 0);
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

	gtaotarget = new OpenGLFramebuffer(screenwidth, screenheight);
	gtaotarget->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R16F, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GL_ASSERT(gtaotarget->Validate());

	glBindFramebuffer(GL_FRAMEBUFFER, gtaotarget->GetFramebuffer());
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &value);
		GL_ASSERT(value == GL_LINEAR);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	blurtarget = new OpenGLFramebuffer(screenwidth, screenheight);
	blurtarget->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R8G8, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	GL_ASSERT(blurtarget->Validate());

	glBindFramebuffer(GL_FRAMEBUFFER, blurtarget->GetFramebuffer());
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &value);
		GL_ASSERT(value == GL_LINEAR);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// accumulation targets
	accumtargets[0] = new OpenGLFramebuffer(screenwidth, screenheight);
	accumtargets[0]->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R8G8, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	GL_ASSERT(accumtargets[0]->Validate());

	accumtargets[1] = new OpenGLFramebuffer(screenwidth, screenheight);
	accumtargets[1]->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_R8G8, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

	GL_ASSERT(accumtargets[1]->Validate());

	// generate noise texture
	unsigned char* data = new unsigned char[16 * 2];

	for( unsigned char i = 0; i < 4; ++i ) {
		for( unsigned char j = 0; j < 4; ++j ) {
			float dirnoise = 0.0625f * ((((i + j) & 0x3) << 2) + (i & 0x3));
			float offnoise = 0.25f * ((j - i) & 0x3);

			data[(i * 4 + j) * 2 + 0] = (unsigned char)(dirnoise * 255.0f);
			data[(i * 4 + j) * 2 + 1] = (unsigned char)(offnoise * 255.0f);
		}
	}

	GL_ASSERT(GLCreateTexture(4, 4, 1, GLFMT_R8G8, &noisetex, data));
	delete[] data;

	// load shaders
	if( !GLCreateEffectFromFile("../media/shadersGL/gbuffer.vert", 0, "../media/shadersGL/gbuffer.frag", &gbuffereffect) )
	{
		MYERROR("Could not load 'gbuffer' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/gtao.frag", &gtaoeffect) )
	{
		MYERROR("Could not load 'GTAO' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/gtaospatialdenoise.frag", &blureffect) )
	{
		MYERROR("Could not load 'spatial denoise' shader");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/gtaotemporaldenoise.frag", &accumeffect) )
	{
		MYERROR("Could not load 'temporal denoise' shader");
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
	gtaoeffect->SetMatrix("matTexture", id);

	gbuffereffect->SetInt("sampler0", 0);
	gbuffereffect->SetInt("sampler1", 1);

	blureffect->SetInt("gtao", 0);
	blureffect->SetInt("depth", 1);
	
	accumeffect->SetInt("historyBuffer", 0);
	accumeffect->SetInt("currIteration", 1);
	accumeffect->SetInt("prevDepthBuffer", 2);
	accumeffect->SetInt("currDepthBuffer", 3);

	combineeffect->SetInt("sampler0", 0);
	combineeffect->SetInt("sampler1", 1);

	gtaoeffect->SetInt("gbufferDepth", 0);
	gtaoeffect->SetInt("gbufferNormals", 1);
	gtaoeffect->SetInt("noise", 2);

	screenquad = new OpenGLScreenQuad();

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8R8G8B8, &text1);

	GLRenderText(
		"Use WASD and mouse to move around\n\n1 - Scene only\n2 - Scene with GTAO (multi-bounce)\n3 - GTAO only\n\nB - Toggle spatial/temporal denoiser\nH - Toggle help text",
		text1, 512, 512);

	// setup camera
	camera.Fov = GLDegreesToRadians(60);
	camera.Aspect = (float)screenwidth / (float)screenheight;
	camera.Far = 50.0f;

	camera.SetEyePosition(-0.12f, 3.0f, 0);
	camera.SetOrientation(-GL_HALF_PI, 0, 0);

	return true;
}

void UninitScene()
{
	gbuffer->Detach(GL_COLOR_ATTACHMENT2);

	SAFE_DELETE(gbuffereffect);
	SAFE_DELETE(gtaoeffect);
	SAFE_DELETE(blureffect);
	SAFE_DELETE(accumeffect);
	SAFE_DELETE(combineeffect);
	SAFE_DELETE(presenteffect);

	SAFE_DELETE(gtaotarget);
	SAFE_DELETE(blurtarget);
	SAFE_DELETE(accumtargets[0]);
	SAFE_DELETE(accumtargets[1]);
	SAFE_DELETE(gbuffer);
	SAFE_DELETE(model);
	SAFE_DELETE(screenquad);

	GL_SAFE_DELETE_TEXTURE(depthbuffers[0]);
	GL_SAFE_DELETE_TEXTURE(depthbuffers[1]);
	GL_SAFE_DELETE_TEXTURE(noisetex);
	GL_SAFE_DELETE_TEXTURE(supplytex);
	GL_SAFE_DELETE_TEXTURE(supplynormalmap);
	GL_SAFE_DELETE_TEXTURE(text1);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
	camera.Event_KeyDown(keycode);
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode == 0x42 ) {	//B
		useblur = !useblur;
	} else if( keycode == 0x48 ) {	// H
		drawtext = !drawtext;
	} else if( keycode >= 0x31 && keycode <= 0x33 ) {
		gtaomode = (keycode - 0x30);
	}

	camera.Event_KeyUp(keycode);
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	camera.Event_MouseMove(dx, dy);
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	camera.Event_MouseDown(button);
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	camera.Event_MouseUp(button);
}

void Update(float delta)
{
	camera.Update(delta);
}

void RenderModelWithGTAO(const float proj[16], float clip[4], float eye[3])
{
	float rotations[6] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
	float offsets[4] = { 0.0f, 0.5f, 0.25f, 0.75f };

	float ocean[] = { 0, 0.0103f, 0.0707f, 1 };
	float black[] = { 0, 0, 0, 1 };
	float white[] = { 1, 1, 1, 1 };

	float projinfo[] = { 2.0f / (screenwidth * proj[0]), 2.0f / (screenheight * proj[5]), -1.0f / proj[0], -1.0f / proj[5] };
	float invres[] = { 1.0f / screenwidth, 1.0f / screenheight, 0, 0 };
	float params[4] = { 0, 0, 0, 0 };

	OpenGLFramebuffer* srctarget = accumtargets[currsample % 2];
	OpenGLFramebuffer* dsttarget = accumtargets[1 - currsample % 2];

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_FRAMEBUFFER_SRGB);	// because textures and attachment0 is sRGB

	// render G-buffer
	gbuffer->Attach(GL_COLOR_ATTACHMENT2, depthbuffers[currsample % 2], 0);
	gbuffer->Set();
	{
		// AMD bug...
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.0f, 0.0103f, 0.0707f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		GLenum buffs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers(3, buffs);

		//glClearBufferfv(GL_COLOR, GL_DRAW_BUFFER0, ocean);
		//glClearBufferfv(GL_COLOR, GL_DRAW_BUFFER1, black);
		//glClearBufferfv(GL_COLOR, GL_DRAW_BUFFER2, white);
		//glClearBufferfv(GL_DEPTH, 0, white);

		gbuffereffect->Begin();
		{
			for( GLuint i = 0; i < model->GetNumSubsets(); ++i )
				model->DrawSubset(i, true);
		}
		gbuffereffect->End();
	}
	gbuffer->Unset();

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_FRAMEBUFFER_SRGB);

	if( gtaomode > 1 )
	{
		// render AO
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(2));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(1));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, noisetex);

		clip[2] = 0.5f * (screenheight / (2.0f * tanf(camera.Fov * 0.5f)));

		params[0] = rotations[currsample % 6] / 360.0f;
		params[1] = offsets[(currsample / 6) % 4];

		gtaoeffect->SetVector("eyePos", eye);
		gtaoeffect->SetVector("projInfo", projinfo);
		gtaoeffect->SetVector("clipInfo", clip);
		gtaoeffect->SetVector("invRes", invres);
		gtaoeffect->SetVector("params", params);

		gtaotarget->Set();
		{
			gtaoeffect->Begin();
			{
				screenquad->Draw();
			}
			gtaoeffect->End();
		}
		gtaotarget->Unset();

		// spatial denoiser
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gtaotarget->GetColorAttachment(0));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(2));

		blureffect->SetVector("invRes", invres);

		blurtarget->Set();
		{
			blureffect->Begin();
			{
				screenquad->Draw();
			}
			blureffect->End();
		}
		blurtarget->Unset();

		// temporal denoiser
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srctarget->GetColorAttachment(0));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, blurtarget->GetColorAttachment(0));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, depthbuffers[1 - currsample % 2]);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, depthbuffers[currsample % 2]);

		dsttarget->Set();
		{
			float invprevV[16];
			float invcurrV[16];

			GLMatrixInverse(invprevV, prevV);
			GLMatrixInverse(invcurrV, currV);

			accumeffect->SetMatrix("matPrevViewInv", invprevV);
			accumeffect->SetMatrix("matCurrViewInv", invcurrV);
			accumeffect->SetMatrix("matPrevView", prevV);
			accumeffect->SetMatrix("matProj", proj);

			float normprojinfo[] = { 1.0f / proj[0], 1.0f / proj[5], 0, 0 };

			accumeffect->SetVector("projInfo", normprojinfo);
			accumeffect->SetVector("clipPlanes", clip);

			accumeffect->Begin();
			{
				screenquad->Draw();
			}
			accumeffect->End();
		}
		dsttarget->Unset();

		currsample = (currsample + 1) % 6;
	}

	// apply to scene
	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	// backbuffer is sRGB (really???), albedo is sRGB, gtao is RGB
	glEnable(GL_FRAMEBUFFER_SRGB);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gbuffer->GetColorAttachment(0));

	glActiveTexture(GL_TEXTURE1);

	if( useblur )
		glBindTexture(GL_TEXTURE_2D, dsttarget->GetColorAttachment(0));
	else
		glBindTexture(GL_TEXTURE_2D, gtaotarget->GetColorAttachment(0));
	
	combineeffect->SetInt("gtaoMode", gtaomode);
	combineeffect->Begin();
	{
		screenquad->Draw();
	}
	combineeffect->End();
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	float world[16];
	float view[16];
	float proj[16];
	float worldview[16];
	float worldviewinv[16];
	float viewproj[16];
	float viewinv[16];
	float viewprojinv[16];
	float currWVP[16];
	float eye[3];
	float clip[4] = { 0, 0, 0, 0 };

	frametime = elapsedtime;

	camera.Animate(alpha);
	camera.GetViewMatrix(view);
	camera.GetProjectionMatrix(proj);
	camera.GetEyePosition(eye);

	clip[0] = camera.Near;
	clip[1] = camera.Far;

	GLMatrixInverse(viewinv, view);
	GLMatrixMultiply(viewproj, view, proj);
	GLMatrixInverse(viewprojinv, viewproj);

	glViewport(0, 0, screenwidth, screenheight);

	// using GTAO with the Sponza palace
	GLMatrixScaling(world, METERS_PER_UNIT, METERS_PER_UNIT, METERS_PER_UNIT);

	world[12] = 60.518921f * METERS_PER_UNIT;
	world[13] = (778.0f - 651.495361f) * METERS_PER_UNIT;
	world[14] = -38.690552f * METERS_PER_UNIT;

	GLMatrixMultiply(worldview, world, view);
	GLMatrixInverse(worldviewinv, worldview);

	GLMatrixMultiply(currWVP, worldview, proj);
	GLMatrixAssign(currV, view);

	if( time == 0.0f ) {
		GLMatrixAssign(prevV, currV);
	}

	gbuffereffect->SetMatrix("matWorldView", worldview);
	gbuffereffect->SetMatrix("matWorldViewInv", worldviewinv);
	gbuffereffect->SetMatrix("matProj", proj);
	gbuffereffect->SetVector("clipPlanes", clip);

	RenderModelWithGTAO(proj, clip, eye);

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

	GLMatrixAssign(prevV, currV);

#ifdef _DEBUG
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";
#endif

	SwapBuffers(hdc);
}
