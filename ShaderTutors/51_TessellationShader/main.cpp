
#include <Windows.h>
#include <GdiPlus.h>

#include <iostream>
#include "../common/gl4x.h"

// helper macros
#define TITLE				"Shader sample 51: Tessellation shader"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define SAFE_DELETE(x)		if( (x) ) { delete (x); (x) = 0; }
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(x[0]))

// external variables
extern HWND		hwnd;
extern HDC		hdc;
extern long		screenwidth;
extern long		screenheight;

// sample variables
OpenGLMesh*		gridmesh			= 0;
OpenGLEffect*	drawcurveeffect		= 0;
OpenGLEffect*	drawgrideffect		= 0;
OpenGLEffect*	drawpointseffect	= 0;
OpenGLEffect*	drawlineseffect		= 0;
GLuint			controlvertices		= 0;
GLuint			controltangents		= 0;
GLuint			inputlayout1		= 0;
GLuint			inputlayout2		= 0;

short			mousedown			= 0;
int				selectedpoint		= -1;
float			selectiondx			= 0;
float			selectiondy			= 0;

// control point data (for 1360x768)
float controlpoints[][4] = {
	{ 200, 100, 0, 0 },
	{ 222, 407, 0, 0 },
	{ 315, 587, 0, 0 },
	{ 684, 304, 0, 0 },

	{ 963, 387, 0, 0 },
	{ 1090, 615, 0, 0 },
	{ 671, 688, 0, 0 },
	{ 710, 507, 0, 0 }
};

float tangents[][4] = {
	{ 127, 162, 0, 0 },
	{ -149, 71, 0, 0 },
	{ 161, 35, 0, 0 },
	{ 135, -89, 0, 0 },

	{ 163, -49, 0, 0 },
	{ -118, 99, 0, 0 },
	{ -95, -69, 0, 0 },
	{ 108, -12, 0, 0 }
};

// sample shaders
const char* drawgridVS = {
	"#version 430\n"

	"subroutine vec4 CalcVertexFunc();\n"

	"in vec3 my_Position;\n"
	"layout(location = 0) uniform mat4 matProj;\n"
	"layout(location = 1) uniform int numControlVertices;\n"

	// NOTE: this is patch data!!! (0, 1, 1, 2, 2, 3, 3, ..., n)
	"layout(std140, binding = 0) readonly buffer ControlVertexData {\n"
	"	vec4 data[];\n"
	"} controlVertices;\n"

	"subroutine uniform CalcVertexFunc vertexFunc;\n"

	"layout(index = 0) subroutine(CalcVertexFunc) vec4 CalcPointVertex() {\n"
	"	return matProj * vec4(my_Position, 1.0);\n"
	"}\n"

	"layout(index = 1) subroutine(CalcVertexFunc) vec4 CalcTangentVertex() {\n"
	"	int index = min(gl_VertexID * 2, numControlVertices - 1);\n"
	"	vec3 pos = controlVertices.data[index].xyz;\n"

	"	return matProj * vec4(pos + my_Position, 1.0);\n"
	"}\n"

	"void main() {\n"
	"	gl_Position = vertexFunc();\n"
	"}\n"
};

const char* drawlinesVS = {
	"#version 430\n"

	"layout(location = 0) uniform mat4 matProj;\n"
	"layout(location = 1) uniform int numControlVertices;\n"

	// NOTE: this is patch data!!! (0, 1, 1, 2, 2, 3, 3, ..., n)
	"layout(std140, binding = 0) readonly buffer ControlVertexData {\n"
	"	vec4 data[];\n"
	"} controlVertices;\n"

	"layout(std140, binding = 1) readonly buffer TangentData {\n"
	"	vec4 data[];\n"
	"} tangents;\n"

	"void main() {\n"
		// this is a cool trick to generate lines from point data
	"	vec4 linevert;\n"

	"	if( gl_VertexID % 2 == 0 ) {\n"
			// use control point
	"		int index = min(gl_VertexID, numControlVertices - 1);\n"
	"		vec3 pos = controlVertices.data[index].xyz;\n"

	"		linevert = matProj * vec4(pos, 1.0);\n"
	"	} else {\n"
			// use tangent
	"		int index = min(gl_VertexID - 1, numControlVertices - 1);\n"
	"		vec3 pos = controlVertices.data[index].xyz;\n"
	"		vec3 tang = tangents.data[gl_VertexID / 2].xyz;\n"

	"		linevert = matProj * vec4(pos + tang, 1.0);\n"
	"	}\n"

	"	gl_Position = linevert;\n"
	"}\n"
};

const char* drawpointsGS = {
	"#version 430\n"

	"layout(location = 2) uniform vec2 pointSize;\n"

	"layout(points) in;\n"
	"layout(triangle_strip, max_vertices = 4) out;\n"

	"void main() {\n"
	"	vec4 pos = gl_in[0].gl_Position;\n"

	"	gl_Position = pos + vec4(-pointSize.x, pointSize.y, 0.0, 0.0) * pos.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(-pointSize.x, -pointSize.y, 0.0, 0.0) * pos.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(pointSize.x, pointSize.y, 0.0, 0.0) * pos.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = pos + vec4(pointSize.x, -pointSize.y, 0.0, 0.0) * pos.w;\n"
	"	EmitVertex();\n"
	"}\n"
};

const char* drawlinesGS = {
	"#version 430\n"

	"layout(location = 2) uniform vec2 lineThickness;\n"

	"layout(lines) in;\n"
	"layout(triangle_strip, max_vertices = 4) out;\n"

	"void main() {\n"
	"	vec4 cpos1 = gl_in[0].gl_Position;\n"
	"	vec4 cpos2 = gl_in[1].gl_Position;\n"

	"	vec4 spos1 = cpos1 / cpos1.w;\n"
	"	vec4 spos2 = cpos2 / cpos2.w;\n"

	"	vec2 d = normalize(spos2.xy - spos1.xy);\n"
	"	vec2 n = vec2(d.y, -d.x);\n"

	"	vec4 v1 = spos1 + vec4(n * lineThickness, 0.0, 0.0);\n"
	"	vec4 v2 = spos1 - vec4(n * lineThickness, 0.0, 0.0);\n"
	"	vec4 v3 = spos2 + vec4(n * lineThickness, 0.0, 0.0);\n"
	"	vec4 v4 = spos2 - vec4(n * lineThickness, 0.0, 0.0);\n"

	"	gl_Position = v1 * cpos1.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = v3 * cpos2.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = v2 * cpos1.w;\n"
	"	EmitVertex();\n"

	"	gl_Position = v4 * cpos2.w;\n"
	"	EmitVertex();\n"
	"}\n"
};

const char* drawcurveVS = {
	"#version 430\n"

	"layout(location = 0) in vec3 my_Position;\n"

	"void main() {\n"
	"	gl_Position = vec4(my_Position, 1.0);\n"
	"}\n"
};

const char* drawcurveTCS = {
	"#version 430\n"

	"layout(vertices = 2) out;\n"
	"void main() {\n"
	"	gl_TessLevelOuter[0] = 1.0;\n"
	"	gl_TessLevelOuter[1] = 16.0;\n"

	"	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
	"}\n"
};

const char* drawcurveTES = {
	"#version 430\n"

	"layout(location = 0) uniform mat4 matProj;\n"

	"layout(std140, binding = 0) readonly buffer TangentData {\n"
	"	vec4 data[];\n"
	"} tangents;\n"

	"layout(isolines, equal_spacing) in;\n"
	"void main() {\n"
	"	float u = gl_TessCoord.x;\n"
	"	int patchID = gl_PrimitiveID;\n"
	
	"	vec4 tan0 = tangents.data[patchID];\n"
	"	vec4 tan1 = tangents.data[patchID + 1];\n"

	"	float h0 = 2.0 * u * u * u - 3.0 * u * u + 1.0;\n"
	"	float h1 = -2.0 * u * u * u + 3.0 * u * u;\n"
	"	float h2 = u * u * u - 2.0 * u * u + u;\n"
	"	float h3 = u * u * u - u * u;\n"

	"	vec4 pos = "
	"		h0 * gl_in[0].gl_Position + "
	"		h1 * gl_in[1].gl_Position + "
	"		h2 * tan0 + "
	"		h3 * tan1;\n"

	"	gl_Position = matProj * vec4(pos.xyz, 1.0);\n"
	"}\n"
};

const char* drawcurveFS = {
	"#version 430\n"

	"layout(location = 3) uniform vec4 color;\n"
	"layout(location = 0) out vec4 my_FragColor0;\n"

	"void main() {\n"
	"	my_FragColor0 = color;\n"
	"}\n"
};

// sample functions
void ConvertPointsToPatches();

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	Quadron::qGLExtensions::QueryFeatures(hdc);

	glClearColor(1, 1, 1, 1);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDisable(GL_DEPTH_TEST);

	// scale control points to screen resolution
	float scalex = screenwidth / 1360.0f;
	float scaley = screenheight / 768.0f;

	for( int i = 0; i < ARRAY_SIZE(controlpoints); ++i ) {
		controlpoints[i][0] *= scalex;
		controlpoints[i][1] *= scaley;

		tangents[i][0] *= scalex;
		tangents[i][1] *= scaley;
	}

	// create shaders
	if( !GLCreateEffectFromMemory(drawgridVS, 0, drawcurveFS, &drawgrideffect) ) {
		MYERROR("Could not create grid effect");
		return false;
	}

	if( !GLCreateEffectFromMemory(drawgridVS, drawpointsGS, drawcurveFS, &drawpointseffect) ) {
		MYERROR("Could not create points effect");
		return false;
	}

	if( !GLCreateEffectFromMemory(drawlinesVS, 0, drawcurveFS, &drawlineseffect) ) {
		MYERROR("Could not create lines effect");
		return false;
	}

	if( !GLCreateTessellationProgramFromMemory(drawcurveVS, drawcurveTCS, drawcurveTES, drawlinesGS, drawcurveFS, &drawcurveeffect) ) {
		MYERROR("Could not create tessellation program");
		return false;
	}

	// create control points
	glGenBuffers(1, &controlvertices);
	
	glBindBuffer(GL_ARRAY_BUFFER, controlvertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(controlpoints) * 2 - 2 * sizeof(float[4]), NULL, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &inputlayout1);
	glBindVertexArray(inputlayout1);
	{
		glBindBuffer(GL_ARRAY_BUFFER, controlvertices);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float[4]), (const void*)0);
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// create tangents
	glGenBuffers(1, &controltangents);

	glBindBuffer(GL_ARRAY_BUFFER, controltangents);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tangents), NULL, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &inputlayout2);
	glBindVertexArray(inputlayout2);
	{
		glBindBuffer(GL_ARRAY_BUFFER, controltangents);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float[4]), (const void*)0);
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	ConvertPointsToPatches();

	// create grid
	OpenGLVertexElement decl[] = {
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	float (*vdata)[3] = 0;
	GLushort* idata = 0;
	GLuint gridlinesx = (screenwidth / 16) - 1;
	GLuint gridlinesy = (screenheight / 16) - 1;

	if( !GLCreateMesh((gridlinesx + gridlinesy) * 2, 0, 0, decl, &gridmesh) ) {
		MYERROR("Could not create grid mesh");
		return false;
	}

	gridmesh->LockVertexBuffer(0, 0, GL_MAP_WRITE_BIT, (void**)&vdata);

	for( GLuint i = 0; i < gridlinesx * 2; i += 2 ) {
		vdata[i][0] = (i / 2 + 1) * 16.0f;
		vdata[i][1] = 0;
		vdata[i][2] = 0;

		vdata[i + 1][0] = (i / 2 + 1) * 16.0f;
		vdata[i + 1][1] = (float)screenheight;
		vdata[i + 1][2] = 0;
	}

	vdata += (gridlinesx * 2);

	for( GLuint i = 0; i < gridlinesy * 2; i += 2 ) {
		vdata[i][0] = 0;
		vdata[i][1] = (i / 2 + 1) * 16.0f;
		vdata[i][2] = 0;

		vdata[i + 1][0] = (float)screenwidth;
		vdata[i + 1][1] = (i / 2 + 1) * 16.0f;
		vdata[i + 1][2] = 0;
	}

	gridmesh->UnlockVertexBuffer();

	gridmesh->GetAttributeTable()[0].PrimitiveType	= GL_LINES;
	gridmesh->GetAttributeTable()[0].VertexStart	= 0;
	gridmesh->GetAttributeTable()[0].VertexCount	= gridmesh->GetNumVertices();

	return true;
}

void UninitScene()
{
	delete drawcurveeffect;
	delete drawpointseffect;
	delete drawlineseffect;
	delete drawgrideffect;
	delete gridmesh;

	glDeleteVertexArrays(1, &inputlayout1);
	glDeleteVertexArrays(1, &inputlayout2);

	glDeleteBuffers(1, &controlvertices);
	glDeleteBuffers(1, &controltangents);

	GLKillAnyRogueObject();
}

void ConvertPointsToPatches()
{
	int numpoints = ARRAY_SIZE(controlpoints);
	int numverts = numpoints * 2 - 2;

	glBindBuffer(GL_ARRAY_BUFFER, controlvertices);
	float (*vdata)[4] = (float(*)[4])glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	{
		// first and last are not duplicated
		GLVec4Assign(vdata[0], controlpoints[0]);
		GLVec4Assign(vdata[numverts - 1], controlpoints[numpoints - 1]);

		// duplicate others
		for( int i = 1, j = 1; i < numpoints - 1; ++i, j += 2 ) {
			GLVec4Assign(vdata[j], controlpoints[i]);
			GLVec4Assign(vdata[j + 1], controlpoints[i]);
		}
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	// update tangents
	glBindBuffer(GL_ARRAY_BUFFER, controltangents);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tangents), tangents);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Event_KeyDown(unsigned char keycode)
{
}

void Event_KeyUp(unsigned char keycode)
{
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	int numpoints = ARRAY_SIZE(controlpoints);
	float radius = 15.0f;
	float dist;
	float mx = (float)x;
	float my = (float)(screenheight - y - 1);

	if( mousedown == 1 ) {
		if( selectedpoint == -1 ) {
			// look for a point
			for( int i = 0; i < numpoints; ++i ) {
				selectiondx = controlpoints[i][0] - mx;
				selectiondy = controlpoints[i][1] - my;
				dist = selectiondx * selectiondx + selectiondy * selectiondy;

				if( dist < radius * radius ) {
					selectedpoint = i;
					break;
				}
			}
		}

		if( selectedpoint == -1 ) {
			// look for a tangent
			for( int i = 0; i < numpoints; ++i ) {
				selectiondx = (controlpoints[i][0] + tangents[i][0]) - mx;
				selectiondy = (controlpoints[i][1] + tangents[i][1]) - my;
				dist = selectiondx * selectiondx + selectiondy * selectiondy;

				if( dist < radius * radius ) {
					selectedpoint = numpoints + i;
					break;
				}
			}
		}

		if( selectedpoint > -1 && selectedpoint < numpoints ) {
			// a point is selected
			controlpoints[selectedpoint][0] = GLMin<float>(GLMax<float>(selectiondx + mx, 0), (float)screenwidth - 1);
			controlpoints[selectedpoint][1] = GLMin<float>(GLMax<float>(selectiondy + my, 0), (float)screenheight - 1);

			ConvertPointsToPatches();
		} else if( selectedpoint >= numpoints && selectedpoint < numpoints * 2 ) {
			// a tangent is selected
			int selectedtangent = selectedpoint - numpoints;

			//float tpx = controlpoints[selectedtangent][0] + tangents[selectedtangent][0];
			//float tpy = controlpoints[selectedtangent][1] + tangents[selectedtangent][1];

			float tpx = GLMin<float>(GLMax<float>(selectiondx + mx, 0), (float)screenwidth - 1);
			float tpy = GLMin<float>(GLMax<float>(selectiondy + my, 0), (float)screenheight - 1);

			tangents[selectedtangent][0] = tpx - controlpoints[selectedtangent][0];
			tangents[selectedtangent][1] = tpy - controlpoints[selectedtangent][1];

			ConvertPointsToPatches();
		}
	} else {
		selectedpoint = -1;
	}
}

void Event_MouseScroll(int x, int y, short dz)
{
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = 1;
	Event_MouseMove(x, y, 0, 0);
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown = 0;
}

void Update(float delta)
{
}

void Render(float alpha, float elapsedtime)
{
	float		proj[16];
	float		black[]				= { 0, 0, 0, 1 };
	float		grey[]				= { 0.75f, 0.75f, 0.75f, 1 };
	float		pointsize[2]		= { 10.0f / screenwidth, 10.0f / screenheight };
	float		curvethickness[2]	= { 3.0f / screenwidth, 3.0f / screenheight };
	
	GLuint		funcindex		= 0;
	GLsizei		numpoints		= ARRAY_SIZE(controlpoints);
	GLsizei		numvertices		= numpoints * 2 - 2;

	OpenGLColor	pointcolor(0xff7470ff);
	OpenGLColor	tangentcolor(0xff74dd70);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glPatchParameteri(GL_PATCH_VERTICES, 2);

	GLMatrixOrthoRH(proj, 0, (float)screenwidth, 0, (float)screenheight, -1, 1);

	// render grid
	drawgrideffect->SetMatrix("matProj", proj);
	drawgrideffect->SetVector("color", grey);
	drawgrideffect->Begin();
	{
		funcindex = 0;
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &funcindex);

		gridmesh->DrawSubset(0);
	}
	drawgrideffect->End();

	// render tangent lines (with a cool shader trick)
	drawlineseffect->SetMatrix("matProj", proj);
	drawlineseffect->SetVector("color", &tangentcolor.r);
	drawlineseffect->Begin();
	{
		glUniform1i(1, numvertices);

		//glBindVertexArray(0);		// NOTE: deprecated
		glBindVertexArray(inputlayout1);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, controlvertices);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, controltangents);

		glDrawArrays(GL_LINES, 0, numpoints * 2);
	}
	drawlineseffect->End();

	// render spline
	drawcurveeffect->SetMatrix("matProj", proj);
	drawcurveeffect->SetVector("lineThickness", curvethickness);
	drawcurveeffect->SetVector("color", black);
	drawcurveeffect->Begin();
	{
		glBindVertexArray(inputlayout1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, controltangents);

		glDrawArrays(GL_PATCHES, 0, numvertices);
	}
	drawcurveeffect->End();

	// render control points
	drawpointseffect->SetMatrix("matProj", proj);
	drawpointseffect->SetVector("pointSize", pointsize);
	drawpointseffect->SetVector("color", &pointcolor.r);
	drawpointseffect->Begin();
	{
		funcindex = 0;
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &funcindex);

		glBindVertexArray(inputlayout1);
		glDrawArrays(GL_POINTS, 0, numvertices);
	}
	drawpointseffect->End();

	// render tangent points
	drawpointseffect->SetVector("color", &tangentcolor.r);
	drawpointseffect->Begin();
	{
		funcindex = 1;
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &funcindex);
		glUniform1i(1, numvertices);

		glBindVertexArray(inputlayout2);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, controlvertices);

		glDrawArrays(GL_POINTS, 0, numpoints);
	}
	drawpointseffect->End();

	// check errors
	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
		std::cout << "Error\n";

	SwapBuffers(hdc);
}
