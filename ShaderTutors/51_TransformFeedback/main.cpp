
#include <Windows.h>
#include <GdiPlus.h>
#include <iostream>

#include "../common/gl4x.h"
#include "../common/basiccamera.h"

// TODO:
// - GPU-based sorting

#define NUM_EMITTERS		2
#define MAX_PARTICLES		8192

#define EMIT_RATE			0.2f
#define LIFE_SPAN			7.0f
#define BUOYANCY			0.002f

#define BUDDHA_SCALE		25.0f
#define BOWL_SCALE			0.005f

#define BOWL1_POSITION		-2.5f, 0.0f, -0.5f
#define BOWL2_POSITION		2.5f, 0.0f, -0.5f

// helper macros
#define TITLE				"Shader sample 51: Transform feedback & program pipelines"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(x[0]))

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample structures
struct Particle
{
	float position[4];
	float velocity[4];	// w = age
	float color[4];
};

// sample variables
BasicCamera		basiccamera;
OpenGLMesh*		ground					= 0;
OpenGLMesh*		buddha					= 0;
OpenGLMesh*		bowl					= 0;
OpenGLMesh*		sky						= 0;
OpenGLEffect*	effect					= 0;
OpenGLEffect*	texeffect				= 0;
OpenGLEffect*	skyeffect				= 0;
Particle*		emitters				= 0;
int				mousedown				= 0;
int				currentbuffer			= 0;

GLuint			emittersbuffer			= 0;
GLuint			particlebuffers[3]		= { 0, 0, 0 };
GLuint			transformfeedbacks[3]	= { 0, 0, 0 };
GLuint			inputlayout				= 0;
GLuint			skytex					= 0;
GLuint			randomtex				= 0;
GLuint			smoketex				= 0;
GLuint			gradienttex				= 0;
GLuint			groundtex				= 0;
GLuint			countquery				= 0;

GLuint			particlevertprogram		= 0;
GLuint			billboardprogram		= 0;
GLuint			smokeemitgeomprogram	= 0;
GLuint			smokeupdatevertprogram	= 0;
GLuint			smokeupdategeomprogram	= 0;

GLuint			smokeemitpipeline		= 0;
GLuint			smokeupdatepipeline		= 0;
GLuint			billboardpipeline		= 0;

// sample shaders
const char* particleVS = {
	"#version 440\n"

	"layout(location = 0) in vec4 particlePos;\n"
	"layout(location = 1) in vec4 particleVel;\n"
	"layout(location = 2) in vec4 particleColor;\n"

	"out vec4 vs_particlePos;\n"
	"out vec4 vs_particleVel;\n"
	"out vec4 vs_particleColor;\n"

	"void main() {\n"
	"	vs_particlePos		= particlePos;\n"
	"	vs_particleVel		= particleVel;\n"
	"	vs_particleColor	= particleColor;\n"
	"}\n"
};

const char* smokeemitGS = {
	"#version 440\n"

	"layout(points) in;\n"
	"layout(points, max_vertices = 40) out;\n"

	"in vec4 vs_particlePos[];\n"
	"in vec4 vs_particleVel[];\n"
	"in vec4 vs_particleColor[];\n"

	"layout(xfb_buffer = 0) out GS_OUTPUT {\n"
	"	layout(xfb_offset = 0) vec4 particlePos;\n"
	"	layout(xfb_offset = 16) vec4 particleVel;\n"
	"	layout(xfb_offset = 32) vec4 particleColor;\n"
	"} gl_out;\n"

	"layout(rgba32f, location = 0, binding = 0) uniform sampler2D randomTex;\n"
	"layout(location = 1) uniform float time;\n"
	"layout(location = 2) uniform float emitRate;\n"

	"vec3 randomVector(float xi) {\n"
	"	return texture(randomTex, vec2(xi, 0.5f)).xyz - vec3(0.5);\n"
	"};\n"

	"float randomScalar(float xi) {\n"
	"	return 2.0 * texture(randomTex, vec2(xi, 0.5f)).x - 1.0;\n"
	"};\n"

	"float randomFloat(float xi) {\n"
	"	return texture(randomTex, vec2(xi, 0.5f)).x;\n"
	"};\n"

	"void main() {\n"
	"	float seed = (time * 123525.0 + gl_PrimitiveIDIn * 1111.0) / 1234.0;\n"
	"	float age = vs_particleVel[0].w;\n"

	"	if( age >= emitRate ) {\n"
	"		for( int i = 0; i < 40; ++i ) {\n"
	"			vec3 vel;\n"

	"			vel.x = 0.45 * randomScalar(seed);\n"
	"			vel.y = 1.0f;\n"
	"			vel.z = 0.45 * randomScalar(seed + 4207.56);\n"

	"			vel = normalize(vel);\n"

	"			gl_out.particlePos.xyz	= vs_particlePos[0].xyz;\n"
	"			gl_out.particlePos.w	= 1.0;\n"
	"			gl_out.particleVel.xyz	= vel * 0.75;\n"
	"			gl_out.particleVel.w	= 0.0;\n"
	"			gl_out.particleColor	= vec4(1.0);\n"

	"			EmitVertex();\n"
	"			seed += 4207.56;\n"
	"		}\n"
	"	}\n"
	"}\n"
};

const char* smokeupdateVS = {
	"#version 440\n"

	"layout(location = 0) in vec4 particlePos;\n"
	"layout(location = 1) in vec4 particleVel;\n"
	"layout(location = 2) in vec4 particleColor;\n"

	"out vec4 vs_particlePos;\n"
	"out vec4 vs_particleVel;\n"
	"out vec4 vs_particleColor;\n"

	"layout(location = 0) uniform float elapsedTime;\n"
	"layout(location = 1) uniform float buoyancy;\n"

	"void main() {\n"
	"	vec3 vel = particleVel.xyz;\n"
	"	vel.y += buoyancy;\n"

	"	vs_particlePos.xyz	= particlePos.xyz + vel * elapsedTime;\n"
	"	vs_particlePos.w	= 1.0;\n"
	"	vs_particleVel.xyz	= vel;\n"
	"	vs_particleVel.w	= particleVel.w + elapsedTime;\n"
	"	vs_particleColor	= particleColor;\n"
	"}\n"
};

const char* smokeupdateGS = {
	"#version 440\n"

	"layout(points) in;\n"
	"layout(points, max_vertices = 120) out;\n"

	"in vec4 vs_particlePos[];\n"
	"in vec4 vs_particleVel[];\n"
	"in vec4 vs_particleColor[];\n"

	"layout(location = 0) uniform float particleLife;\n"

	"layout(xfb_buffer = 0) out GS_OUTPUT {\n"
	"	layout(xfb_offset = 0) vec4 particlePos;\n"
	"	layout(xfb_offset = 16) vec4 particleVel;\n"
	"	layout(xfb_offset = 32) vec4 particleColor;\n"
	"} gl_out;\n"

	"void main() {\n"
	"	float age = vs_particleVel[0].w;\n"

	"	if( age < particleLife ) {\n"
	"		gl_out.particlePos		= vs_particlePos[0];\n"
	"		gl_out.particleVel		= vs_particleVel[0];\n"
	"		gl_out.particleColor	= vs_particleColor[0];\n"

	"		EmitVertex();\n"
	"	}\n"
	"}\n"
};

const char* billboardGS = {
	"#version 440\n"

	"layout(location = 0) uniform mat4 matWVP;\n"
	"layout(location = 1) uniform mat4 matWorldView;\n"
	"layout(location = 2) uniform vec2 clipRadius;\n"
	"layout(location = 3) uniform float particleLife;\n"

	"layout(points) in;\n"
	"layout(triangle_strip, max_vertices = 4) out;\n"

	"in vec4 vs_particlePos[];\n"
	"in vec4 vs_particleVel[];\n"
	"in vec4 vs_particleColor[];\n"

	"out gl_PerVertex {\n"
	"	vec4 gl_Position;\n"
	"};\n"

	"out vec3 tex;\n"

	"void main() {\n"
	"	vec4 pos = matWVP * vs_particlePos[0];\n"
	"	vec4 vpos = matWorldView * vs_particlePos[0];\n"

	"	float age = vs_particleVel[0].w;\n"
	"	float normage = age / particleLife;\n"

	"	vec2 screenradius = clipRadius / vpos.z;\n"

	"	gl_Position = pos + vec4(-screenradius.x, screenradius.y, 0.0, 0.0) * pos.w;\n"
	"	tex = vec3(0.0, 1.0, normage);\n"

	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(-screenradius.x, -screenradius.y, 0.0, 0.0) * pos.w;\n"
	"	tex = vec3(0.0, 0.0, normage);\n"

	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(screenradius.x, screenradius.y, 0.0, 0.0) * pos.w;\n"
	"	tex = vec3(1.0, 1.0, normage);\n"

	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(screenradius.x, -screenradius.y, 0.0, 0.0) * pos.w;\n"
	"	tex = vec3(1.0, 0.0, normage);\n"

	"	EmitVertex();\n"
	"}\n"
};

const char* billboardFS = {
	"#version 440\n"

	"layout(location = 4, binding = 0) uniform sampler3D sampler0;\n"
	"layout(location = 5, binding = 1) uniform sampler2D sampler1;\n"

	"in vec3 tex;\n"

	"layout(location = 0) out vec4 my_FragColor0;\n"

	"void main() {\n"
	"	my_FragColor0 = texture(sampler0, tex) * texture(sampler1, vec2(tex.z, 0.5));\n"
	"}\n"
};

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures(hdc);

	if( Quadron::qGLExtensions::GLVersion < Quadron::qGLExtensions::GL_4_4 ) {
		std::cout << "This sample requires at least GL 4.4\n";
		return false;
	}

	wglSwapInterval(1);

	glClearColor(0.0f, 0.0103f, 0.0707f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);

	// create input layout
	glGenVertexArrays(1, &inputlayout);
	glBindVertexArray(inputlayout);
	{
		glBindVertexBuffers(0, 1, NULL, NULL, NULL);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribBinding(0, 0);
		glVertexAttribBinding(1, 0);
		glVertexAttribBinding(2, 0);

		glVertexAttribFormat(0, 4, GL_FLOAT, GL_FALSE, 0);
		glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 16);
		glVertexAttribFormat(2, 4, GL_FLOAT, GL_FALSE, 32);
	}
	glBindVertexArray(0);

	// create buffers & transform feedbacks
	glGenBuffers(1, &emittersbuffer);
	glGenBuffers(3, particlebuffers);
	glGenTransformFeedbacks(3, transformfeedbacks);

	emitters = new Particle[NUM_EMITTERS];
	memset(emitters, 0, NUM_EMITTERS * sizeof(Particle));

	glBindBuffer(GL_ARRAY_BUFFER, emittersbuffer);
	glBufferData(GL_ARRAY_BUFFER, NUM_EMITTERS * sizeof(Particle), NULL, GL_DYNAMIC_DRAW);

	for( int i = 0; i < 3; ++i ) {
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformfeedbacks[i]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, particlebuffers[i]);
	}

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	glBindBuffer(GL_ARRAY_BUFFER, particlebuffers[0]);
	glBufferStorage(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_DYNAMIC_STORAGE_BIT|GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);

	glBindBuffer(GL_ARRAY_BUFFER, particlebuffers[1]);
	glBufferStorage(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_DYNAMIC_STORAGE_BIT|GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);

	glBindBuffer(GL_ARRAY_BUFFER, particlebuffers[2]);
	glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// create random texture
	float* data = new float[2048 * 4];

	for( int i = 0; i < 2048 * 4; ++i )
		data[i] = GLRandomFloat();

	glGenTextures(1, &randomtex);

	glBindTexture(GL_TEXTURE_2D, randomtex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2048, 1, 0, GL_RGBA, GL_FLOAT, data);

	delete[] data;

	// create emit/update pipelines
	particlevertprogram = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &particleVS);
	smokeemitgeomprogram = glCreateShaderProgramv(GL_GEOMETRY_SHADER, 1, &smokeemitGS);

	GL_ASSERT(particlevertprogram != 0);
	GL_ASSERT(smokeemitgeomprogram != 0);

	GL_ASSERT(GLCheckLinkStatus(particlevertprogram));
	GL_ASSERT(GLCheckLinkStatus(smokeemitgeomprogram));

	glGenProgramPipelines(1, &smokeemitpipeline);

	glUseProgramStages(smokeemitpipeline, GL_VERTEX_SHADER_BIT, particlevertprogram);
	glUseProgramStages(smokeemitpipeline, GL_GEOMETRY_SHADER_BIT, smokeemitgeomprogram);

	smokeupdatevertprogram = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &smokeupdateVS);
	smokeupdategeomprogram = glCreateShaderProgramv(GL_GEOMETRY_SHADER, 1, &smokeupdateGS);

	GL_ASSERT(smokeupdatevertprogram != 0);
	GL_ASSERT(smokeupdategeomprogram != 0);

	GL_ASSERT(GLCheckLinkStatus(smokeupdatevertprogram));
	GL_ASSERT(GLCheckLinkStatus(smokeupdategeomprogram));

	glGenProgramPipelines(1, &smokeupdatepipeline);

	glUseProgramStages(smokeupdatepipeline, GL_VERTEX_SHADER_BIT, smokeupdatevertprogram);
	glUseProgramStages(smokeupdatepipeline, GL_GEOMETRY_SHADER_BIT, smokeupdategeomprogram);

	// create billboard pipeline
	billboardprogram = glCreateProgram();

	GLuint geomshader = GLCompileShaderFromMemory(GL_GEOMETRY_SHADER, billboardGS, 0);
	GLuint fragshader = GLCompileShaderFromMemory(GL_FRAGMENT_SHADER, billboardFS, 0);

	GL_ASSERT(geomshader != 0);
	GL_ASSERT(fragshader != 0);

	glAttachShader(billboardprogram, geomshader);
	glAttachShader(billboardprogram, fragshader);

	glProgramParameteri(billboardprogram, GL_PROGRAM_SEPARABLE, GL_TRUE);
	glLinkProgram(billboardprogram);

	GL_ASSERT(GLCheckLinkStatus(billboardprogram));

	glDeleteShader(geomshader);
	glDeleteShader(fragshader);

	glGenProgramPipelines(1, &billboardpipeline);
	glUseProgramStages(billboardpipeline, GL_VERTEX_SHADER_BIT, particlevertprogram);
	glUseProgramStages(billboardpipeline, GL_GEOMETRY_SHADER_BIT|GL_FRAGMENT_SHADER_BIT, billboardprogram);

	// load smoke texture
	GLCreateVolumeTextureFromFile("../media/textures/smokevol1.dds", true, &smoketex);

	glBindTexture(GL_TEXTURE_3D, smoketex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// load gradient texture
	GLCreateTextureFromFile("../media/textures/colorgradient.dds", true, &gradienttex);

	glBindTexture(GL_TEXTURE_2D, gradienttex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// create query
	glGenQueries(1, &countquery);

	// setup scene
	GLCreateMeshFromQM("../media/meshes/cylinder.qm", &ground);
	GLCreateMeshFromQM("../media/meshes/happy1.qm", &buddha);
	GLCreateMeshFromQM("../media/meshes/bowl.qm", &bowl);
	GLCreateMeshFromQM("../media/meshes10/sky.qm", &sky);

	GLCreateEffectFromFile("../media/shadersGL/lambert.vert", 0, "../media/shadersGL/lambert.frag", &effect);
	GLCreateEffectFromFile("../media/shadersGL/lambert.vert", 0, "../media/shadersGL/lambert.frag", &texeffect, "#define TEXTURED\n");
	GLCreateEffectFromFile("../media/shadersGL/sky.vert", 0, "../media/shadersGL/sky.frag", &skyeffect);

	GLCreateTextureFromFile("../media/textures/wood2.jpg", true, &groundtex);
	GLCreateCubeTextureFromFile("../media/textures/sky4.dds", true, &skytex);

	// setup camera
	basiccamera.SetAspect((float)screenwidth / screenheight);
	basiccamera.SetFov(GLDegreesToRadians(80));
	basiccamera.SetClipPlanes(0.1f, 30.0f);
	basiccamera.SetDistance(4);
	basiccamera.SetOrientation(GLDegreesToRadians(-175), GLDegreesToRadians(15), 0);

	const OpenGLAABox& box = buddha->GetBoundingBox();
	float center = (box.Max[1] - box.Min[1]) * 0.5f * BUDDHA_SCALE;

	basiccamera.SetPosition(0, center - 0.2f, 0);

	return true;
}

void UninitScene()
{
	delete ground;
	delete buddha;
	delete bowl;
	delete sky;
	delete effect;
	delete texeffect;
	delete skyeffect;

	delete[] emitters;

	glDeleteProgramPipelines(1, &smokeemitpipeline);
	glDeleteProgramPipelines(1, &smokeupdatepipeline);
	glDeleteProgramPipelines(1, &billboardpipeline);

	glDeleteProgram(particlevertprogram);
	glDeleteProgram(smokeemitgeomprogram);
	glDeleteProgram(smokeupdatevertprogram);
	glDeleteProgram(smokeupdategeomprogram);
	glDeleteProgram(billboardprogram);

	glDeleteVertexArrays(1, &inputlayout);
	glDeleteTransformFeedbacks(3, transformfeedbacks);
	glDeleteQueries(1, &countquery);

	glDeleteBuffers(1, &emittersbuffer);
	glDeleteBuffers(3, particlebuffers);

	glDeleteTextures(1, &groundtex);
	glDeleteTextures(1, &randomtex);
	glDeleteTextures(1, &gradienttex);
	glDeleteTextures(1, &smoketex);
	glDeleteTextures(1, &skytex);

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
	if( mousedown & 1 ) {
		basiccamera.OrbitRight(GLDegreesToRadians(dx));
		basiccamera.OrbitUp(GLDegreesToRadians(dy));
	}

	if( mousedown & 4 ) {
		float scale = basiccamera.GetDistance() / 10.0f;
		float amount = 1e-3f + scale * (0.1f - 1e-3f);

		basiccamera.PanRight(dx * -amount);
		basiccamera.PanUp(dy * amount);
	}
}

void Event_MouseScroll(int x, int y, short dz)
{
	float dist = basiccamera.GetDistance();

	dist = GLClamp(dist - dz * 0.5f, 0.5f, 10.0f);
	basiccamera.SetDistance(dist);
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown |= button;
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown &= (~button);
}

void Update(float delta)
{
	basiccamera.Update(delta);
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;
	static bool prevbufferusable = false;

	float view[16];
	float proj[16];
	float viewproj[16];
	float eye[4] = { 0, 0, 0, 1 };
	float vdir[3];

	float clipradius[2];
	float hfov = GLHorizontalFov(basiccamera.GetFov(), (float)screenwidth, (float)screenheight);
	
	clipradius[0] = 0.4f / tanf(hfov * 0.5f);
	clipradius[1] = 0.4f / tanf(basiccamera.GetFov() * 0.5f);

	time += elapsedtime;

	basiccamera.Animate(alpha);
	basiccamera.GetViewMatrix(view);
	basiccamera.GetProjectionMatrix(proj);
	basiccamera.GetEyePosition(eye);

	GLMatrixMultiply(viewproj, view, proj);

	// update emitters
	for( int i = 0; i < NUM_EMITTERS; ++i ) {
		emitters[i].velocity[3] = ((emitters[i].velocity[3] > EMIT_RATE) ? 0.0f : (emitters[i].velocity[3] + elapsedtime));

		if( i == 0 ) {
			GLVec4Set(emitters[i].position, BOWL1_POSITION, 1);
			emitters[i].position[1] += 0.75f;
		} else if( i == 1 ) {
			GLVec4Set(emitters[i].position, BOWL2_POSITION, 1);
			emitters[i].position[1] += 0.75f;
		} else {
			assert(false);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, emittersbuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_EMITTERS * sizeof(Particle), emitters);

	// emit particles
	GLintptr offset = 0;
	GLsizei stride = sizeof(Particle);

	glEnable(GL_RASTERIZER_DISCARD);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformfeedbacks[2]);
	{
		glBindTexture(GL_TEXTURE_2D, randomtex);
		glProgramUniform1f(smokeemitgeomprogram, 1, time);
		glProgramUniform1f(smokeemitgeomprogram, 2, EMIT_RATE);

		glBindProgramPipeline(smokeemitpipeline);
		glBindVertexArray(inputlayout);
		glBindVertexBuffers(0, 1, &emittersbuffer, &offset, &stride);

		glBeginTransformFeedback(GL_POINTS);
		{
			glDrawArrays(GL_POINTS, 0, NUM_EMITTERS);
		}
		glEndTransformFeedback();
	}

	// update particles
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformfeedbacks[currentbuffer]);
	{
		glProgramUniform1f(smokeupdatevertprogram, 0, elapsedtime);
		glProgramUniform1f(smokeupdatevertprogram, 1, BUOYANCY);
		glProgramUniform1f(smokeupdategeomprogram, 0, LIFE_SPAN);

		glBindProgramPipeline(smokeupdatepipeline);
		glBindVertexArray(inputlayout);
		glBindVertexBuffers(0, 1, &particlebuffers[2], &offset, &stride);

		glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, countquery);
		glBeginTransformFeedback(GL_POINTS);
		{
			// update and add newly generated particles
			glDrawTransformFeedback(GL_POINTS, transformfeedbacks[2]);

			// update and add older particles
			if( prevbufferusable ) {
				glBindVertexBuffers(0, 1, &particlebuffers[1 - currentbuffer], &offset, &stride);
				glDrawTransformFeedback(GL_POINTS, transformfeedbacks[1 - currentbuffer]);
			}
		}
		glEndTransformFeedback();
		glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
	}
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
	glDisable(GL_RASTERIZER_DISCARD);

	// sort particles
	GLuint count = 0;
	glGetQueryObjectuiv(countquery, GL_QUERY_RESULT, &count);

	basiccamera.GetPosition(vdir);

	GLVec3Subtract(vdir, vdir, eye);
	GLVec3Normalize(vdir, vdir);

	if( count > 0 )
	{
		glBindBuffer(GL_ARRAY_BUFFER, particlebuffers[currentbuffer]);
		Particle* data = (Particle*)glMapBufferRange(GL_ARRAY_BUFFER, 0, count * sizeof(Particle), GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);
		{
			Particle tmp;

			auto Distance = [&](const Particle& p) -> float {
				float pdir[3];
				GLVec3Subtract(pdir, p.position, eye);

				return GLVec3Dot(pdir, vdir);
			};

			for( GLuint i = 1; i < count; ++i ) {
				GLuint j = i;

				while( j > 0 && Distance(data[j - 1]) < Distance(data[j]) ) {
					tmp = data[j - 1];
					data[j - 1] = data[j];
					data[j] = tmp;

					--j;
				}
			}
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	// render scene
	float		world[16];
	float		worldinv[16];
	float		tmp[16];
	float		lightpos[4]	= { 1, 0.65f, -0.5f, 0 }; //{ 1, 0.3f, -0.5f, 0 };
	float		ambient[4]	= { 0.01f, 0.01f, 0.01f, 1 };
	OpenGLColor	color(1.0f, 0.782f, 0.344f, 1.0f);

	glEnable(GL_FRAMEBUFFER_SRGB);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	// sky
	GLMatrixScaling(world, 20, 20, 20);

	world[12] = eye[0];
	world[13] = eye[1];
	world[14] = eye[2];

	GLMatrixAssign(tmp, proj);

	// project to depth ~ 1.0
	tmp[10] = -1.0f + 1e-4f;
	tmp[14] = 0;

	GLMatrixMultiply(tmp, view, tmp);

	skyeffect->SetVector("eyePos", eye);
	skyeffect->SetMatrix("matWorld", world);
	skyeffect->SetMatrix("matViewProj", tmp);

	skyeffect->Begin();
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, skytex);
		sky->DrawSubset(0);
	}
	skyeffect->End();

	// ground
	GLMatrixRotationAxis(tmp, GLDegreesToRadians(40), 0, 1, 0);
	GLMatrixScaling(world, 10, 0.1f, 10);
	GLMatrixMultiply(world, world, tmp);

	world[13] = -0.05f;

	GLMatrixInverse(worldinv, world);
	GLMatrixScaling(tmp, 3, 3, 1);

	texeffect->SetMatrix("matWorld", world);
	texeffect->SetMatrix("matWorldInv", worldinv);
	texeffect->SetMatrix("matTexture", tmp);
	texeffect->SetMatrix("matViewProj", viewproj);
	texeffect->SetVector("lightPos", lightpos);
	texeffect->SetVector("matAmbient", ambient);
	texeffect->SetInt("sampler0", 0);

	texeffect->Begin();
	{
		glBindTexture(GL_TEXTURE_2D, groundtex);
		ground->Draw();
	}
	texeffect->End();

	// buddha + bowls
	GLMatrixScaling(world, BUDDHA_SCALE, BUDDHA_SCALE, BUDDHA_SCALE);

	world[13] = buddha->GetBoundingBox().Min[1] * -BUDDHA_SCALE;
	world[14] = 1.5f;

	GLMatrixInverse(worldinv, world);

	effect->SetMatrix("matWorld", world);
	effect->SetMatrix("matWorldInv", worldinv);
	effect->SetMatrix("matViewProj", viewproj);
	effect->SetVector("lightPos", lightpos);
	effect->SetVector("matAmbient", ambient);

	effect->Begin();
	{
		for( GLuint i = 0; i < buddha->GetNumSubsets(); ++i ) {
			effect->SetVector("matDiffuse", &color.r);
			effect->CommitChanges();

			buddha->DrawSubset(i);
		}

		GLMatrixScaling(world, BOWL_SCALE, BOWL_SCALE, BOWL_SCALE);

		GLVec3Set(&world[12], BOWL1_POSITION);
		GLMatrixInverse(worldinv, world);

		effect->SetMatrix("matWorld", world);
		effect->SetMatrix("matWorldInv", worldinv);

		for( GLuint i = 0; i < bowl->GetNumSubsets(); ++i ) {
			effect->SetVector("matDiffuse", &bowl->GetMaterialTable()[i].Diffuse.r);
			effect->CommitChanges();

			bowl->DrawSubset(i);
		}

		GLVec3Set(&world[12], BOWL2_POSITION);
		GLMatrixInverse(worldinv, world);

		effect->SetMatrix("matWorld", world);
		effect->SetMatrix("matWorldInv", worldinv);

		for( GLuint i = 0; i < bowl->GetNumSubsets(); ++i ) {
			effect->SetVector("matDiffuse", &bowl->GetMaterialTable()[i].Diffuse.r);
			effect->CommitChanges();

			bowl->DrawSubset(i);
		}
	}
	effect->End();

	glUseProgram(0);

	// render smoke
	glProgramUniformMatrix4fv(billboardprogram, 0, 1, GL_FALSE, viewproj);
	glProgramUniformMatrix4fv(billboardprogram, 1, 1, GL_FALSE, view);
	glProgramUniform2fv(billboardprogram, 2, 1, clipradius);
	glProgramUniform1f(billboardprogram, 3, LIFE_SPAN);
	glProgramUniform1i(billboardprogram, 4, 0);
	glProgramUniform1i(billboardprogram, 5, 1);

	glBindProgramPipeline(billboardpipeline);
	glBindVertexArray(inputlayout);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, smoketex);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gradienttex);

	glBindVertexBuffers(0, 1, &particlebuffers[currentbuffer], &offset, &stride);
	glDrawTransformFeedback(GL_POINTS, transformfeedbacks[currentbuffer]);
	
	glActiveTexture(GL_TEXTURE0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_FRAMEBUFFER_SRGB);

	currentbuffer = 1 - currentbuffer;
	prevbufferusable = true;

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

	SwapBuffers(hdc);
}
