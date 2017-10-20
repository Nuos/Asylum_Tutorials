
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>
#include <cstddef>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

#define GRID_SIZE		20		// static batching won't work for more
#define OBJECT_SCALE	0.75f

// helper macros
#define TITLE				"Shader sample 51: Instancing methods"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
BasicCamera			basiccamera;

OpenGLMesh*			mesh			= 0;
OpenGLMesh*			staticbatch		= 0;	// for static instancing
OpenGLEffect*		kitcheneffect	= 0;
OpenGLEffect*		instanceeffect	= 0;
OpenGLEffect*		basic2D			= 0;
OpenGLScreenQuad*	screenquad		= 0;

GLuint				hwinstancevao	= 0;
GLuint				instancebuffer	= 0;
GLuint				text1			= 0;
int					rendermethod	= 1;
bool				mousedown		= false;

// sample functions
void CreateStaticBatch();
void CreateInstancedLayout();

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
	glDepthFunc(GL_LESS);

	if( !GLCreateMeshFromQM("../media/meshes/beanbag2.qm", &mesh) ) {
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/blinnphong.vert", 0, "../media/shadersGL/blinnphong.frag", &kitcheneffect) ) {
		MYERROR("Could not load 'blinnphong' effect");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/blinnphong.vert", 0, "../media/shadersGL/blinnphong.frag", &instanceeffect, "#define HARDWARE_INSTANCING\n") ) {
		MYERROR("Could not load 'instanced blinnphong' effect");
		return false;
	}

	if( !GLCreateEffectFromFile("../media/shadersGL/basic2D.vert", 0, "../media/shadersGL/basic2D.frag", &basic2D) ) {
		MYERROR("Could not load 'basic2D' shader");
		return false;
	}

	// setup static instancing
	std::cout << "Generating static batch...";
	CreateStaticBatch();
	std::cout << "done\n";

	// setup hardware instancing
	CreateInstancedLayout();

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8B8G8R8, &text1);

	GLRenderText(
		"1 - Kitchen instancing\n2 - Static instancing\n3 - Hardware instancing",
		text1, 512, 512);

	screenquad = new OpenGLScreenQuad();

	// setup camera
	basiccamera.SetAspect((float)screenwidth / screenheight);
	basiccamera.SetFov(GLDegreesToRadians(80));
	basiccamera.SetClipPlanes(0.1f, 30.0f);
	basiccamera.SetDistance(GRID_SIZE);
	basiccamera.SetOrientation(GLDegreesToRadians(-155), GLDegreesToRadians(30), 0);

	return true;
}

void CreateStaticBatch()
{
	OpenGLAttributeRange*	subsettable			= mesh->GetAttributeTable();
	OpenGLAttributeRange*	batchsubsettable	= new OpenGLAttributeRange[mesh->GetNumSubsets()];
	OpenGLCommonVertex*		vdata				= 0;
	OpenGLCommonVertex*		batchvdata			= 0;
	GLushort*				idata				= 0;
	GLuint*					batchidata			= 0;
	GLuint					numsubsets			= mesh->GetNumSubsets();
	GLuint					count				= GRID_SIZE * GRID_SIZE * GRID_SIZE;
	float					world[16], worldinv[16];

	OpenGLVertexElement elem[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0, 12, GLDECLTYPE_FLOAT3, GLDECLUSAGE_NORMAL, 0 },
		{ 0, 24, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	GLCreateMesh(mesh->GetNumVertices() * count, mesh->GetNumIndices() * count, GLMESH_32BIT, elem, &staticbatch);

	mesh->LockVertexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&vdata);
	mesh->LockIndexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&idata);

	GLMatrixScaling(world, OBJECT_SCALE, OBJECT_SCALE, OBJECT_SCALE);

	staticbatch->LockVertexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchvdata);
	staticbatch->LockIndexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchidata);
	{
		// pre-transform vertices
		for( int i = 0; i < GRID_SIZE; ++i ) {
			for( int j = 0; j < GRID_SIZE; ++j ) {
				for( int k = 0; k < GRID_SIZE; ++k ) {
					world[12] = GRID_SIZE * -0.5f + i;
					world[13] = GRID_SIZE * -0.5f + j;
					world[14] = GRID_SIZE * -0.5f + k;

					GLMatrixInverse(worldinv, world);

					for( GLuint l = 0; l < mesh->GetNumVertices(); ++l ) {
						const OpenGLCommonVertex& vert = vdata[l];
						OpenGLCommonVertex& batchvert = batchvdata[l];

						GLVec3TransformCoord(&batchvert.x, &vert.x, world);
						GLVec3TransformTranspose(&batchvert.nx, worldinv, &vert.nx);
					}

					batchvdata += mesh->GetNumVertices();
				}
			}
		}

		// group indices by subset
		GLuint indexoffset = 0;

		for( GLuint s = 0; s < numsubsets; ++s ) {
			const OpenGLAttributeRange& subset = subsettable[s];
			OpenGLAttributeRange& batchsubset = batchsubsettable[s];

			for( int i = 0; i < GRID_SIZE; ++i ) {
				for( int j = 0; j < GRID_SIZE; ++j ) {
					for( int k = 0; k < GRID_SIZE; ++k ) {
						GLuint vertexoffset = (i * GRID_SIZE * GRID_SIZE + j * GRID_SIZE + k) * mesh->GetNumVertices();

						for( GLuint l = 0; l < subset.IndexCount; ++l )
							batchidata[l] = idata[subset.IndexStart + l] + vertexoffset;

						batchidata += subset.IndexCount;
					}
				}
			}

			batchsubset.AttribId		= subset.AttribId;
			batchsubset.Enabled			= subset.Enabled;
			batchsubset.PrimitiveType	= subset.PrimitiveType;

			batchsubset.IndexStart		= indexoffset;
			batchsubset.IndexCount		= subset.IndexCount * count;
			batchsubset.VertexStart		= 0;
			batchsubset.VertexCount		= 0;

			indexoffset += count * subset.IndexCount;
		}
	}
	staticbatch->UnlockIndexBuffer();
	staticbatch->UnlockVertexBuffer();

	staticbatch->SetAttributeTable(batchsubsettable, numsubsets);

	mesh->UnlockIndexBuffer();
	mesh->UnlockVertexBuffer();

	delete[] batchsubsettable;
}

void CreateInstancedLayout()
{
	float world[16];
	GLuint instancebuffersize = GRID_SIZE * GRID_SIZE * GRID_SIZE * 80;

	glGenBuffers(1, &instancebuffer);
	glBindBuffer(GL_ARRAY_BUFFER, instancebuffer);
	glBufferData(GL_ARRAY_BUFFER, instancebuffersize, NULL, GL_STATIC_DRAW);

	GLMatrixScaling(world, OBJECT_SCALE, OBJECT_SCALE, OBJECT_SCALE);

	float* mdata = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	
	for( int i = 0; i < GRID_SIZE; ++i ) {
		for( int j = 0; j < GRID_SIZE; ++j ) {
			for( int k = 0; k < GRID_SIZE; ++k ) {
				world[12] = GRID_SIZE * -0.5f + i;
				world[13] = GRID_SIZE * -0.5f + j;
				world[14] = GRID_SIZE * -0.5f + k;

				GLMatrixAssign(mdata, world);
				mdata += 16;

				GLVec4Set(mdata, 1, 1, 1, 1);
				mdata += 4;
			}
		}
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glGenVertexArrays(1, &hwinstancevao);
	glBindVertexArray(hwinstancevao);
	{
		// vertex layout
		glBindBuffer(GL_ARRAY_BUFFER, mesh->GetVertexBuffer());

		glEnableVertexAttribArray(GLDECLUSAGE_POSITION);
		glEnableVertexAttribArray(GLDECLUSAGE_NORMAL);
		glEnableVertexAttribArray(GLDECLUSAGE_TEXCOORD);

		glVertexAttribPointer(GLDECLUSAGE_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(OpenGLCommonVertex), (const GLvoid*)0);
		glVertexAttribPointer(GLDECLUSAGE_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(OpenGLCommonVertex), (const GLvoid*)12);
		glVertexAttribPointer(GLDECLUSAGE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(OpenGLCommonVertex), (const GLvoid*)24);

		// instance layout
		glBindBuffer(GL_ARRAY_BUFFER, instancebuffer);

		glEnableVertexAttribArray(6);
		glEnableVertexAttribArray(7);
		glEnableVertexAttribArray(8);
		glEnableVertexAttribArray(9);
		glEnableVertexAttribArray(10);

		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 80, (const GLvoid*)0);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 80, (const GLvoid*)16);
		glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, 80, (const GLvoid*)32);
		glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, 80, (const GLvoid*)48);
		glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, 80, (const GLvoid*)64);

		glVertexAttribDivisor(6, 1);
		glVertexAttribDivisor(7, 1);
		glVertexAttribDivisor(8, 1);
		glVertexAttribDivisor(9, 1);
		glVertexAttribDivisor(10, 1);
	}
	glBindVertexArray(0);
}

void UninitScene()
{
	delete staticbatch;
	delete mesh;
	delete kitcheneffect;
	delete instanceeffect;
	delete basic2D;
	delete screenquad;

	glDeleteVertexArrays(1, &hwinstancevao);

	GL_SAFE_DELETE_BUFFER(instancebuffer);
	GL_SAFE_DELETE_TEXTURE(text1);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode >= 0x31 && keycode <= 0x33 )
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
	float identity[16];
	float world[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float eye[4] = { 0, 0, 0, 1 };
	float lightpos[4] = { 6, 3, -10, 1 };
	float color[4] = { 1, 1, 1, 1 };

	GLVec3Scale(lightpos, lightpos, GRID_SIZE / 5.0f);

	if( rendermethod == 1 )
		GLVec4Set(color, 1, 0, 0, 1);
	else if( rendermethod == 2 )
		GLVec4Set(color, 1, 1, 0, 1);
	else if( rendermethod == 3 )
		GLVec4Set(color, 0, 1, 0, 1);

	basiccamera.Animate(alpha);

	basiccamera.GetViewMatrix(view);
	basiccamera.GetProjectionMatrix(proj);
	basiccamera.GetEyePosition(eye);

	GLMatrixMultiply(viewproj, view, proj);
	GLMatrixScaling(world, OBJECT_SCALE, OBJECT_SCALE, OBJECT_SCALE);
	GLMatrixIdentity(identity);

	OpenGLEffect* effect = ((rendermethod == 3) ? instanceeffect : kitcheneffect);

	effect->SetMatrix("matWorld", identity);
	effect->SetMatrix("matViewProj", viewproj);
	effect->SetVector("lightPos", lightpos);
	effect->SetVector("eyePos", eye);
	effect->SetVector("color", color);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	effect->Begin();
	{
		if( rendermethod == 1 ) {
			for( int i = 0; i < GRID_SIZE; ++i ) {
				for( int j = 0; j < GRID_SIZE; ++j ) {
					for( int k = 0; k < GRID_SIZE; ++k ) {
						world[12] = GRID_SIZE * -0.5f + i;
						world[13] = GRID_SIZE * -0.5f + j;
						world[14] = GRID_SIZE * -0.5f + k;

						effect->SetMatrix("matWorld", world);
						effect->CommitChanges();

						mesh->Draw();
					}
				}
			}
		} else if( rendermethod == 2 ) {
			staticbatch->Draw();
		} else if( rendermethod == 3 ) {
			glBindVertexArray(hwinstancevao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GetIndexBuffer());

			const OpenGLAttributeRange* subsettable = mesh->GetAttributeTable();

			for( GLuint i = 0; i < mesh->GetNumSubsets(); ++i ) {
				const OpenGLAttributeRange& subset = subsettable[i];
				GLuint start = subset.IndexStart * ((mesh->GetIndexType() == GL_UNSIGNED_INT) ? 4 : 2);

				// this is where we tell how many instances to render
				glDrawElementsInstanced(subset.PrimitiveType, subset.IndexCount, mesh->GetIndexType(), (char*)0 + start, GRID_SIZE * GRID_SIZE * GRID_SIZE);
			}
		}
	}
	effect->End();

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
