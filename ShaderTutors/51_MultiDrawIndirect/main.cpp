
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>
#include <cstddef>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

#define GRID_SIZE		10		// static batching won't work for more
#define BEANBAG_SCALE	0.75f
#define TEAPOT_SCALE	0.35f
#define ANGEL_SCALE		0.003f

// helper macros
#define TITLE				"Shader sample 51: Multi-draw indirect"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
BasicCamera			basiccamera;

OpenGLMesh*			meshes[3]			= { 0, 0, 0 };
OpenGLMesh*			staticbatches[3]	= { 0, 0, 0 };	// for static batching
OpenGLMesh*			multibatch			= 0;			// for multi-draw
OpenGLEffect*		kitcheneffect		= 0;
OpenGLEffect*		instanceeffect		= 0;
OpenGLEffect*		basic2D				= 0;
OpenGLScreenQuad*	screenquad			= 0;

GLuint				computeprogram		= 0;
GLuint				hwinstancevao		= 0;
GLuint				instancebuffer		= 0;
GLuint				indirectbuffer		= 0;
GLuint				parameterbuffer		= 0;
GLuint				text1				= 0;

float				meshscales[3]		= { BEANBAG_SCALE, TEAPOT_SCALE, ANGEL_SCALE };
float				meshcolors[3][4]	= { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } };
int					rendermethod		= 1;
bool				mousedown			= false;

// sample shaders
const char* generatorCS = {
	"#version 430\n"

	"struct DrawElementsIndirectCommand {\n"
	"	uint count;\n"
	"	uint instanceCount;\n"
	"	uint firstIndex;\n"
	"	uint baseVertex;\n"
	"	uint baseInstance;\n"
	"};\n"

	"layout(std430, binding = 0) writeonly buffer IndirectCommands {\n"
	"	DrawElementsIndirectCommand data[3];\n"
	"} commands;\n"

	"layout(std430, binding = 1) writeonly buffer ParameterBuffer {\n"
	"	int count;\n"
	"} numIndirectCommands;\n"

	"layout(location = 2) uniform float time;\n"
	"layout(location = 3) uniform uvec4 params;\n"

	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;"
	"void main() {\n"
	"	float localtime = mod(time, 3.0);\n"
	"	int count = 3;\n"

	"	if( localtime < 1.0 )\n"
	"		count = 1;\n"
	"	else if( localtime < 2.0 )\n"
	"		count = 2;\n"

	"	uint maxnuminstances = (params.w / 3);\n"
	"	float freq1 = sin(time * 2.0) * 0.5 + 0.5;\n"
	"	float freq2 = cos(time * 4.0) * 0.5 + 0.5;\n"
	"	float freq3 = (sin(time) + cos(time)) * 0.3535533 + 0.5;\n"

	"	uint numinstances1 = uint(freq1 * float(maxnuminstances));\n"
	"	uint numinstances2 = uint(freq2 * float(maxnuminstances));\n"
	"	uint numinstances3 = uint(freq3 * float(maxnuminstances));\n"

	"	commands.data[0].count			= params.x;\n"
	"	commands.data[0].instanceCount	= numinstances1;\n"
	"	commands.data[0].firstIndex		= 0;\n"
	"	commands.data[0].baseVertex		= 0;\n"
	"	commands.data[0].baseInstance	= 0;\n"

	"	commands.data[1].count			= params.y;\n"
	"	commands.data[1].instanceCount	= numinstances2;\n"
	"	commands.data[1].firstIndex		= params.x;\n"
	"	commands.data[1].baseVertex		= 0;\n"
	"	commands.data[1].baseInstance	= maxnuminstances + 1;\n"

	"	commands.data[2].count			= params.z;\n"
	"	commands.data[2].instanceCount	= numinstances3;\n"
	"	commands.data[2].firstIndex		= params.x + params.y;\n"
	"	commands.data[2].baseVertex		= 0;\n"
	"	commands.data[2].baseInstance	= 2 * maxnuminstances + 1;\n"

	"	numIndirectCommands.count = count;\n"
	"}\n"
};

// sample functions
void CreateMultiDrawBatch();
void CreateStaticBatches();
void CreateIndirectCommandBuffer();
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

	if( !GLCreateMeshFromQM("../media/meshes/beanbag2.qm", &meshes[0]) ) {
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes/teapot.qm", &meshes[1]) ) {
		MYERROR("Could not load mesh");
		return false;
	}

	if( !GLCreateMeshFromQM("../media/meshes/angel.qm", &meshes[2]) ) {
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

	// setup static batching
	std::cout << "Generating static batch...";
	CreateStaticBatches();
	std::cout << "done\n";

	std::cout << "Generating multi-draw static batch...";
	CreateMultiDrawBatch();
	std::cout << "done\n";

	// setup indirect draw
	CreateIndirectCommandBuffer();

	// setup hardware instancing
	CreateInstancedLayout();

	// render text
	GLCreateTexture(512, 512, 1, GLFMT_A8B8G8R8, &text1);

	if( Quadron::qGLExtensions::ARB_indirect_parameters && Quadron::qGLExtensions::ARB_compute_shader ) {
		GLRenderText(
			"1 - Kitchen draw\n2 - Static batching\n3 - Multi-draw (selective draw from SB)\n4 - Multi-draw indirect (instanced SBing)\n5 - Indirect parameters",
			text1, 512, 512);

		computeprogram = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, &generatorCS);
		GL_ASSERT(GLCheckLinkStatus(computeprogram));
	} else {
		GLRenderText(
			"1 - Kitchen draw\n2 - Static batching\n3 - Multi-draw (selective draw from SB)\n4 - Multi-draw indirect (instanced SBing)",
			text1, 512, 512);
	}

	screenquad = new OpenGLScreenQuad();

	// setup camera
	basiccamera.SetAspect((float)screenwidth / screenheight);
	basiccamera.SetFov(GLDegreesToRadians(80));
	basiccamera.SetClipPlanes(0.1f, 30.0f);
	basiccamera.SetDistance(GRID_SIZE);
	basiccamera.SetOrientation(GLDegreesToRadians(-155), GLDegreesToRadians(30), 0);

	return true;
}

void CreateMultiDrawBatch()
{
	OpenGLCommonVertex*		vdata				= 0;
	OpenGLCommonVertex*		batchvdata			= 0;
	GLushort*				idata				= 0;
	GLuint*					batchidata			= 0;
	GLuint					numvertices			= 0;
	GLuint					numindices			= 0;
	float					world[16], worldinv[16];

	for( int m = 0; m < 3; ++m ) {
		numvertices += meshes[m]->GetNumVertices();
		numindices += meshes[m]->GetNumIndices();
	}

	OpenGLVertexElement elem[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0, 12, GLDECLTYPE_FLOAT3, GLDECLUSAGE_NORMAL, 0 },
		{ 0, 24, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	GLCreateMesh(numvertices, numindices, GLMESH_32BIT, elem, &multibatch);

	multibatch->LockVertexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchvdata);
	multibatch->LockIndexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchidata);
	{
		GLuint vertexoffset = 0;

		for( int m = 0; m < 3; ++m ) {
			meshes[m]->LockVertexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&vdata);
			meshes[m]->LockIndexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&idata);

			GLMatrixScaling(world, meshscales[m], meshscales[m], meshscales[m]);
			GLMatrixInverse(worldinv, world);

			// pre-scale vertices
			for( GLuint l = 0; l < meshes[m]->GetNumVertices(); ++l ) {
				const OpenGLCommonVertex& vert = vdata[l];
				OpenGLCommonVertex& batchvert = batchvdata[l];

				GLVec3TransformCoord(&batchvert.x, &vert.x, world);
				GLVec3TransformTranspose(&batchvert.nx, worldinv, &vert.nx);
			}

			batchvdata += meshes[m]->GetNumVertices();

			for( GLuint l = 0; l < meshes[m]->GetNumIndices(); ++l ) {
				batchidata[l] = idata[l] + vertexoffset;
			}

			batchidata += meshes[m]->GetNumIndices();
			vertexoffset += meshes[m]->GetNumVertices();

			meshes[m]->UnlockIndexBuffer();
			meshes[m]->UnlockVertexBuffer();
		}
	}
	multibatch->UnlockIndexBuffer();
	multibatch->UnlockVertexBuffer();

	// NOTE: ignore subset table
}

void CreateStaticBatches()
{
	GLuint totalcount = GRID_SIZE * GRID_SIZE * GRID_SIZE;

	for( int m = 0; m < 3; ++m )
	{
		OpenGLAttributeRange*	subsettable			= meshes[m]->GetAttributeTable();
		OpenGLAttributeRange*	batchsubsettable	= new OpenGLAttributeRange[meshes[m]->GetNumSubsets()];
		OpenGLCommonVertex*		vdata				= 0;
		OpenGLCommonVertex*		batchvdata			= 0;
		GLushort*				idata				= 0;
		GLuint*					batchidata			= 0;
		GLuint					numsubsets			= meshes[m]->GetNumSubsets();
		GLuint					count				= totalcount / 3 + ((m == 0) ? 1 : 0);
		float					world[16], worldinv[16];

		OpenGLVertexElement elem[] =
		{
			{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
			{ 0, 12, GLDECLTYPE_FLOAT3, GLDECLUSAGE_NORMAL, 0 },
			{ 0, 24, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
			{ 0xff, 0, 0, 0, 0 }
		};

		GLCreateMesh(meshes[m]->GetNumVertices() * count, meshes[m]->GetNumIndices() * count, GLMESH_32BIT, elem, &staticbatches[m]);

		meshes[m]->LockVertexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&vdata);
		meshes[m]->LockIndexBuffer(0, 0, GL_MAP_READ_BIT, (void**)&idata);

		GLMatrixScaling(world, meshscales[m], meshscales[m], meshscales[m]);

		staticbatches[m]->LockVertexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchvdata);
		staticbatches[m]->LockIndexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&batchidata);
		{
			// pre-transform vertices
			for( int i = 0; i < GRID_SIZE; ++i ) {
				for( int j = 0; j < GRID_SIZE; ++j ) {
					for( int k = 0; k < GRID_SIZE; ++k ) {
						GLuint index = i * GRID_SIZE * GRID_SIZE + j * GRID_SIZE + k;

						if( index % 3 != m )
							continue;

						world[12] = GRID_SIZE * -0.5f + i;
						world[13] = GRID_SIZE * -0.5f + j;
						world[14] = GRID_SIZE * -0.5f + k;

						GLMatrixInverse(worldinv, world);

						for( GLuint l = 0; l < meshes[m]->GetNumVertices(); ++l ) {
							const OpenGLCommonVertex& vert = vdata[l];
							OpenGLCommonVertex& batchvert = batchvdata[l];

							GLVec3TransformCoord(&batchvert.x, &vert.x, world);
							GLVec3TransformTranspose(&batchvert.nx, worldinv, &vert.nx);
						}

						batchvdata += meshes[m]->GetNumVertices();
					}
				}
			}

			// group indices by subset
			GLuint indexoffset = 0;

			for( GLuint s = 0; s < numsubsets; ++s ) {
				const OpenGLAttributeRange& subset = subsettable[s];
				OpenGLAttributeRange& batchsubset = batchsubsettable[s];

				for( GLuint i = 0; i < count; ++i ) {
					GLuint vertexoffset = i * meshes[m]->GetNumVertices();

					for( GLuint l = 0; l < subset.IndexCount; ++l )
						batchidata[l] = idata[subset.IndexStart + l] + vertexoffset;

					batchidata += subset.IndexCount;
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
		staticbatches[m]->UnlockIndexBuffer();
		staticbatches[m]->UnlockVertexBuffer();

		staticbatches[m]->SetAttributeTable(batchsubsettable, numsubsets);

		meshes[m]->UnlockIndexBuffer();
		meshes[m]->UnlockVertexBuffer();

		delete[] batchsubsettable;
	}
}

void CreateIndirectCommandBuffer()
{
	GLuint totalcount = GRID_SIZE * GRID_SIZE * GRID_SIZE;

	glGenBuffers(1, &indirectbuffer);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectbuffer);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, 3 * sizeof(DrawElementsIndirectCommand), NULL, GL_STATIC_DRAW);

	DrawElementsIndirectCommand* cmddata = (DrawElementsIndirectCommand*)glMapBuffer(GL_DRAW_INDIRECT_BUFFER, GL_WRITE_ONLY);
	{
		cmddata[0].count			= meshes[0]->GetNumIndices();
		cmddata[0].instanceCount	= (totalcount / 3) + 1;
		cmddata[0].firstIndex		= 0;
		cmddata[0].baseVertex		= 0;
		cmddata[0].baseInstance		= 0;

		cmddata[1].count			= meshes[1]->GetNumIndices();
		cmddata[1].instanceCount	= (totalcount / 3);
		cmddata[1].firstIndex		= meshes[0]->GetNumIndices();
		cmddata[1].baseVertex		= 0;
		cmddata[1].baseInstance		= (totalcount / 3) + 1;

		cmddata[2].count			= meshes[2]->GetNumIndices();
		cmddata[2].instanceCount	= (totalcount / 3);
		cmddata[2].firstIndex		= meshes[0]->GetNumIndices() + meshes[1]->GetNumIndices();
		cmddata[2].baseVertex		= 0;
		cmddata[2].baseInstance		= 2 * (totalcount / 3) + 1;
	}
	glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	// for ARB_indirect_parameters
	glGenBuffers(1, &parameterbuffer);

	glBindBuffer(GL_PARAMETER_BUFFER, parameterbuffer);
	glBufferData(GL_PARAMETER_BUFFER, sizeof(GLsizei), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_PARAMETER_BUFFER, 0);
}

void CreateInstancedLayout()
{
	float world[16];
	GLuint totalcount = GRID_SIZE * GRID_SIZE * GRID_SIZE;
	GLuint instancebuffersize = totalcount * 80;

	glGenBuffers(1, &instancebuffer);
	glBindBuffer(GL_ARRAY_BUFFER, instancebuffer);
	glBufferData(GL_ARRAY_BUFFER, instancebuffersize, NULL, GL_STATIC_DRAW);

	GLMatrixIdentity(world);

	float* mdata = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	
	GLuint counts[] = {
		totalcount / 3 + 1,
		totalcount / 3,
		totalcount / 3
	};

	for( GLuint m = 0; m < 3; ++m ) {
		for( GLuint l = 0; l < counts[m]; ++l ) {
			GLuint index = l * 3 + m;

			GLuint i = index / (GRID_SIZE * GRID_SIZE);
			GLuint j = (index % (GRID_SIZE * GRID_SIZE)) / GRID_SIZE;
			GLuint k = index % GRID_SIZE;

			world[12] = GRID_SIZE * -0.5f + i;
			world[13] = GRID_SIZE * -0.5f + j;
			world[14] = GRID_SIZE * -0.5f + k;

			GLMatrixAssign(mdata, world);
			mdata += 16;

			GLVec4Assign(mdata, meshcolors[(index + 2) % 3]);
			mdata += 4;
		}
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glGenVertexArrays(1, &hwinstancevao);
	glBindVertexArray(hwinstancevao);
	{
		// vertex layout
		glBindBuffer(GL_ARRAY_BUFFER, multibatch->GetVertexBuffer());

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
	delete staticbatches[0];
	delete staticbatches[1];
	delete staticbatches[2];
	delete multibatch;
	delete meshes[0];
	delete meshes[1];
	delete meshes[2];
	delete kitcheneffect;
	delete instanceeffect;
	delete basic2D;
	delete screenquad;

	glDeleteProgram(computeprogram);
	glDeleteVertexArrays(1, &hwinstancevao);

	GL_SAFE_DELETE_BUFFER(instancebuffer);
	GL_SAFE_DELETE_BUFFER(indirectbuffer);
	GL_SAFE_DELETE_BUFFER(parameterbuffer);
	GL_SAFE_DELETE_TEXTURE(text1);

	GLKillAnyRogueObject();
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
	unsigned char maxkey = ((computeprogram != 0) ? 0x35 : 0x34);

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
	static float time = 0;

	float identity[16];
	float world[16];
	float view[16];
	float proj[16];
	float viewproj[16];
	float eye[4] = { 0, 0, 0, 1 };
	float lightpos[4] = { 6, 3, -10, 1 };
	float white[4] = { 1, 1, 1, 1 };

	GLVec3Scale(lightpos, lightpos, GRID_SIZE / 5.0f);

	basiccamera.Animate(alpha);

	basiccamera.GetViewMatrix(view);
	basiccamera.GetProjectionMatrix(proj);
	basiccamera.GetEyePosition(eye);

	GLMatrixMultiply(viewproj, view, proj);
	GLMatrixIdentity(identity);

	if( rendermethod == 5 ) {
		// generate indirect commands dynamically
		glProgramUniform1f(computeprogram, 2, time);
		glProgramUniform4ui(computeprogram, 3, meshes[0]->GetNumIndices(), meshes[1]->GetNumIndices(), meshes[2]->GetNumIndices(), GRID_SIZE * GRID_SIZE * GRID_SIZE);
		glUseProgram(computeprogram);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, indirectbuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, parameterbuffer);

		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT); //|GL_PARAMETER_BARRIER_BIT

		time += elapsedtime;
	}

	// render
	OpenGLEffect* effect = ((rendermethod >= 4) ? instanceeffect : kitcheneffect);

	effect->SetMatrix("matWorld", identity);
	effect->SetMatrix("matViewProj", viewproj);
	effect->SetVector("lightPos", lightpos);
	effect->SetVector("eyePos", eye);
	effect->SetVector("color", white);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	effect->Begin();
	{
		if( rendermethod == 1 ) {
			for( int i = 0; i < GRID_SIZE; ++i ) {
				for( int j = 0; j < GRID_SIZE; ++j ) {
					for( int k = 0; k < GRID_SIZE; ++k ) {
						GLuint index = i * GRID_SIZE * GRID_SIZE + j * GRID_SIZE + k;
						float scale = meshscales[index % 3];

						GLMatrixScaling(world, scale, scale, scale);

						world[12] = GRID_SIZE * -0.5f + i;
						world[13] = GRID_SIZE * -0.5f + j;
						world[14] = GRID_SIZE * -0.5f + k;

						effect->SetMatrix("matWorld", world);
						effect->SetVector("color", meshcolors[index % 3]);
						effect->CommitChanges();
						
						meshes[index % 3]->Draw();
					}
				}
			}
		} else if( rendermethod == 2 ) {
			effect->SetVector("color", meshcolors[1]);
			effect->CommitChanges();

			staticbatches[0]->Draw();

			effect->SetVector("color", meshcolors[2]);
			effect->CommitChanges();

			staticbatches[1]->Draw();

			effect->SetVector("color", meshcolors[0]);
			effect->CommitChanges();

			staticbatches[2]->Draw();
		} else if( rendermethod == 3 ) {
			glBindVertexArray(multibatch->GetVertexLayout());
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, multibatch->GetIndexBuffer());

			// render beanbag and angel
			GLsizei counts[2] = { meshes[0]->GetNumIndices(), meshes[2]->GetNumIndices() };
			const GLvoid* indexoffsets[2] = { (const GLvoid*)0, (const GLvoid*)((meshes[0]->GetNumIndices() + meshes[1]->GetNumIndices()) * sizeof(GLuint)) };

			GLMatrixIdentity(world);

			for( int i = 0; i < GRID_SIZE; ++i ) {
				for( int j = 0; j < GRID_SIZE; ++j ) {
					for( int k = 0; k < GRID_SIZE; ++k ) {
						GLuint index = i * GRID_SIZE * GRID_SIZE + j * GRID_SIZE + k;

						if( index % 3 != 1 )
							continue;

						world[12] = GRID_SIZE * -0.5f + i;
						world[13] = GRID_SIZE * -0.5f + j;
						world[14] = GRID_SIZE * -0.5f + k;

						effect->SetMatrix("matWorld", world);
						effect->SetVector("color", meshcolors[((index * 2) / 3) % 3]);
						effect->CommitChanges();

						glMultiDrawElements(GL_TRIANGLES, counts, multibatch->GetIndexType(), &indexoffsets[0], 2);
					}
				}
			}
		} else if( rendermethod == 4 ) {
			glBindVertexArray(hwinstancevao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, multibatch->GetIndexBuffer());
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectbuffer);

			glMultiDrawElementsIndirect(GL_TRIANGLES, multibatch->GetIndexType(), 0, 3, sizeof(DrawElementsIndirectCommand));
		} else if( rendermethod == 5 ) {
			glBindVertexArray(hwinstancevao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, multibatch->GetIndexBuffer());
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectbuffer);
			glBindBuffer(GL_PARAMETER_BUFFER, parameterbuffer);

			glMultiDrawElementsIndirectCount(GL_TRIANGLES, multibatch->GetIndexType(), 0, 0, 3, sizeof(DrawElementsIndirectCommand));
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
