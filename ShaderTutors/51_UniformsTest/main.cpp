
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>
#include <cstddef>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"
#include "../common/spectatorcamera.h"

#define GRID_SIZE		10
#define UNIFORM_COPIES	512
#define OBJECT_SCALE	0.35f

// helper macros
#define TITLE				"Shader sample 51: Uniform data streaming methods"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// uniform blocks
struct EffectUniformBlock
{
	// byte offset 0
	struct VertexUniformData {
		float matWorld[16];
		float matViewProj[16];
		float lightPos[4];
		float eyePos[4];
	} vsuniforms;	// 160 B

	float padding1[4 * 6];	// 96 B

	// byte offset 256
	struct FragmentUniformData {
		float color[4];
	} fsuniforms;	// 16 B

	float padding2[4 * 15];	// 240 B
};

// sample variables
BasicCamera			basiccamera;
EffectUniformBlock	uniformDTO;
EffectUniformBlock*	persistentdata	= 0;

OpenGLFramebuffer*	framebuffer		= 0;	// simulate that buffer updates happen inside a render pass
OpenGLMesh*			mesh			= 0;
OpenGLEffect*		effect1			= 0;
OpenGLEffect*		effect2			= 0;
OpenGLEffect*		basic2D			= 0;
OpenGLScreenQuad*	screenquad		= 0;

GLuint				uniformbuffer1	= 0;	// simple UBO for one block
GLuint				uniformbuffer2	= 0;	// ringbuffer
GLuint				uniformbuffer3	= 0;	// ringbuffer
GLuint				text1			= 0;
int					rendermethod	= 1;
int					currentcopy		= 0;	// current block in the ringbuffer
bool				mousedown		= false;
bool				isGL4_4			= false;

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures();

	isGL4_4 = (Quadron::qGLExtensions::GLVersion >= Quadron::qGLExtensions::GL_4_4);

	// setup opengl
	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	if( !GLCreateMeshFromQM("../media/meshes/teapot.qm", &mesh) )
	{
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/blinnphong.vert", 0, "../media/shadersGL/blinnphong.frag", &effect1) )
	{
		MYERROR("Could not load 'blinnphong' effect");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/uniformbuffer.vert", 0, "../media/shadersGL/uniformbuffer.frag", &effect2) )
	{
		MYERROR("Could not load 'uniformbuffer' effect");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &basic2D) )
	{
		MYERROR("Could not load 'basic2D' shader");
		return false;
	}

	// create framebuffer
	framebuffer = new OpenGLFramebuffer(screenwidth, screenheight);

	framebuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GLFMT_A8B8G8R8, GL_NEAREST);
	framebuffer->AttachRenderbuffer(GL_DEPTH_STENCIL_ATTACHMENT, GLFMT_D24S8);

	if( !framebuffer->Validate() )
	{
		MYERROR("Framebuffer validation failed");
		return false;
	}

	// create uniform buffer
	effect2->SetUniformBlockBinding("VertexUniformData", 0);
	effect2->SetUniformBlockBinding("FragmentUniformData", 1);

	// simple uniform buffer
	glGenBuffers(1, &uniformbuffer1);
	glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer1);

	glBufferData(GL_UNIFORM_BUFFER, sizeof(EffectUniformBlock), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// uniform ringbuffer
	glGenBuffers(1, &uniformbuffer2);
	glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer2);

	glBufferData(GL_UNIFORM_BUFFER, UNIFORM_COPIES * sizeof(EffectUniformBlock), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	if( isGL4_4 ) {
		// uniform storage buffer
		glGenBuffers(1, &uniformbuffer3);
		glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer3);

		glBufferStorage(GL_UNIFORM_BUFFER, UNIFORM_COPIES * sizeof(EffectUniformBlock), NULL, GL_DYNAMIC_STORAGE_BIT|GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	memset(&uniformDTO, 0, sizeof(uniformDTO));

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8B8G8R8, &text1);

	if( isGL4_4 ) {
		glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer3);
		persistentdata = (EffectUniformBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, UNIFORM_COPIES * sizeof(EffectUniformBlock), GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		assert(persistentdata != 0);

		GLRenderText(
			"1 - glUniformXX\n2 - UBO with glBufferSubData\n3 - UBO with glMapBufferRange\n4 - UBO with glFenceSync\n5 - UBO with persistent mapping",
			text1, 512, 512);
	} else {
		GLRenderText(
			"1 - glUniformXX\n2 - UBO with glBufferSubData\n3 - UBO with glMapBufferRange\n4 - UBO with glFenceSync",
			text1, 512, 512);
	}

	screenquad = new OpenGLScreenQuad();

	// setup cameras
	basiccamera.SetAspect((float)screenwidth / screenheight);
	basiccamera.SetFov(GLDegreesToRadians(80));
	basiccamera.SetClipPlanes(0.1f, 30.0f);
	basiccamera.SetDistance(10.0f);

	return true;
}

void UninitScene()
{
	if (persistentdata) {
		glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer3);
		glUnmapBuffer(GL_UNIFORM_BUFFER);

		persistentdata = 0;
	}

	delete mesh;
	delete effect1;
	delete effect2;
	delete basic2D;
	delete screenquad;
	delete framebuffer;

	GL_SAFE_DELETE_BUFFER(uniformbuffer1);
	GL_SAFE_DELETE_BUFFER(uniformbuffer2);
	GL_SAFE_DELETE_BUFFER(uniformbuffer3);
	GL_SAFE_DELETE_TEXTURE(text1);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
	unsigned char maxkey = (isGL4_4 ? 0x35 : 0x34);

	if( keycode >= 0x31 && keycode <= maxkey )
		rendermethod = keycode - 0x30;
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if (mousedown) {
		basiccamera.OrbitRight(GLDegreesToRadians(dx));
		basiccamera.OrbitUp(GLDegreesToRadians(dy));
	}
}

void Event_MouseScroll(int x, int y, short dz)
{
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = true;
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown = false;
}

void Update(float delta)
{
	basiccamera.Update(delta);
}

void Render(float alpha, float elapsedtime)
{
	float world[16];
	float view[16];
	float proj[16];
	float eye[3];

	basiccamera.Animate(alpha);

	basiccamera.GetViewMatrix(view);
	basiccamera.GetProjectionMatrix(proj);
	basiccamera.GetEyePosition(eye);

	GLVec4Set(uniformDTO.vsuniforms.lightPos, 6, 3, 10, 1);
	GLVec4Set(uniformDTO.vsuniforms.eyePos, eye[0], eye[1], eye[2], 1);

	GLMatrixMultiply(uniformDTO.vsuniforms.matViewProj, view, proj);
	GLMatrixScaling(uniformDTO.vsuniforms.matWorld, OBJECT_SCALE, OBJECT_SCALE, OBJECT_SCALE);

	switch( rendermethod ) {
	case 1:	GLVec4Set(uniformDTO.fsuniforms.color, 1, 0, 0, 1); break;
	case 2:	GLVec4Set(uniformDTO.fsuniforms.color, 1, 0.5f, 0, 1); break;
	case 3:	GLVec4Set(uniformDTO.fsuniforms.color, 1, 1, 0, 1); break;
	case 4:	GLVec4Set(uniformDTO.fsuniforms.color, 0, 0.75f, 0, 1); break;
	case 5:	GLVec4Set(uniformDTO.fsuniforms.color, 0, 1, 0, 1); break;

	default:
		break;
	}

	// render pass
	OpenGLEffect*	effect = ((rendermethod > 1) ? effect2 : effect1);
	GLsync			sync = 0;

	if( rendermethod == 1 ) {
		effect1->SetMatrix("matViewProj", uniformDTO.vsuniforms.matViewProj);
		effect1->SetVector("lightPos", uniformDTO.vsuniforms.lightPos);
		effect1->SetVector("eyePos", uniformDTO.vsuniforms.eyePos);
		effect1->SetVector("color", uniformDTO.fsuniforms.color);
	}

	framebuffer->Set();
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	effect->Begin();
	{
		for( int i = 0; i < GRID_SIZE; ++i )
		{
			for( int j = 0; j < GRID_SIZE; ++j )
			{
				for( int k = 0; k < GRID_SIZE; ++k )
				{
					if( currentcopy >= UNIFORM_COPIES ) {
						if( rendermethod == 4 || rendermethod == 2 ) {
							sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
						} else if( rendermethod == 5 ) {
							glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer3);
							glUnmapBuffer(GL_UNIFORM_BUFFER);

							persistentdata = (EffectUniformBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, UNIFORM_COPIES * sizeof(EffectUniformBlock),
								GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);

							assert(persistentdata != 0);

							glBindBuffer(GL_UNIFORM_BUFFER, 0);
							currentcopy = 0;
						}
					}

					uniformDTO.vsuniforms.matWorld[12] = GRID_SIZE * -0.5f + i;
					uniformDTO.vsuniforms.matWorld[13] = GRID_SIZE * -0.5f + j;
					uniformDTO.vsuniforms.matWorld[14] = GRID_SIZE * -0.5f + k;

					if( rendermethod == 1 ) {
						effect1->SetMatrix("matWorld", uniformDTO.vsuniforms.matWorld);
						effect1->CommitChanges();
					} else {
						if( rendermethod == 2 ) {
							if( sync != 0 ) {
								glWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
								glDeleteSync(sync);

								sync = 0;
							}

							glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer1);
							glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(EffectUniformBlock), &uniformDTO);

							glBindBufferRange(GL_UNIFORM_BUFFER, 0, uniformbuffer1, 0, sizeof(EffectUniformBlock::VertexUniformData));
							glBindBufferRange(GL_UNIFORM_BUFFER, 1, uniformbuffer1, offsetof(EffectUniformBlock, fsuniforms), sizeof(EffectUniformBlock::FragmentUniformData));
						} else if( rendermethod == 3 ) {
							glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer1);
							void* data = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(EffectUniformBlock), GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT);
							{
								memcpy(data, &uniformDTO, sizeof(EffectUniformBlock));
							}
							glUnmapBuffer(GL_UNIFORM_BUFFER);

							glBindBufferRange(GL_UNIFORM_BUFFER, 0, uniformbuffer1, 0, sizeof(EffectUniformBlock::VertexUniformData));
							glBindBufferRange(GL_UNIFORM_BUFFER, 1, uniformbuffer1, offsetof(EffectUniformBlock, fsuniforms), sizeof(EffectUniformBlock::FragmentUniformData));
						} else if( rendermethod == 4 ) {
							GLintptr baseoffset = currentcopy * sizeof(EffectUniformBlock);
							GLbitfield flags = GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT;

							if( sync != 0 ) {
								GLenum result = 0;
								GLbitfield waitflags = GL_SYNC_FLUSH_COMMANDS_BIT;

								do {
									result = glClientWaitSync(sync, waitflags, 500000);
									waitflags = 0;

									if( result == GL_WAIT_FAILED ) {
										std::cout << "glClientWaitSync() failed!\n";
										break;
									}
								} while( result == GL_TIMEOUT_EXPIRED );

								glDeleteSync(sync);
								sync = 0;

								currentcopy = 0;
								baseoffset = 0;
							}

							glBindBuffer(GL_UNIFORM_BUFFER, uniformbuffer2);
							
							void* data = glMapBufferRange(GL_UNIFORM_BUFFER, baseoffset, sizeof(EffectUniformBlock), flags);
							assert(data != 0);
							{
								memcpy(data, &uniformDTO, sizeof(EffectUniformBlock));
							}
							glUnmapBuffer(GL_UNIFORM_BUFFER);

							glBindBufferRange(GL_UNIFORM_BUFFER, 0, uniformbuffer2, baseoffset, sizeof(EffectUniformBlock::VertexUniformData));
							glBindBufferRange(GL_UNIFORM_BUFFER, 1, uniformbuffer2, baseoffset + offsetof(EffectUniformBlock, fsuniforms), sizeof(EffectUniformBlock::FragmentUniformData));

							++currentcopy;
						} else if( rendermethod == 5 ) {
							GLintptr baseoffset = currentcopy * sizeof(EffectUniformBlock);
							memcpy(persistentdata + currentcopy, &uniformDTO, sizeof(EffectUniformBlock));

							glBindBufferRange(GL_UNIFORM_BUFFER, 0, uniformbuffer3, baseoffset, sizeof(EffectUniformBlock::VertexUniformData));
							glBindBufferRange(GL_UNIFORM_BUFFER, 1, uniformbuffer3, baseoffset + offsetof(EffectUniformBlock, fsuniforms), sizeof(EffectUniformBlock::FragmentUniformData));

							++currentcopy;
						}
					}

					mesh->DrawSubset(0);
				}
			}
		}
	}
	effect->End();
	framebuffer->Unset();

	// present
	GLMatrixIdentity(world);

	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	basic2D->SetMatrix("matTexture", world);
	basic2D->Begin();
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, framebuffer->GetColorAttachment(0));

		screenquad->Draw();
	}
	basic2D->End();

	// render text
	glViewport(5, screenheight - 517, 512, 512);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float xzplane[4] = { 0, 1, 0, -0.5f };
	GLMatrixReflect(world, xzplane);

	basic2D->SetMatrix("matTexture", world);
	basic2D->Begin();
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, text1);

		screenquad->Draw();
	}
	basic2D->End();

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glViewport(0, 0, screenwidth, screenheight);

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

	SwapBuffers(hdc);
}
