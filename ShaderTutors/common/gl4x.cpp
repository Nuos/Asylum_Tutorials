
#include "gl4x.h"
#include "dds.h"

#include <iostream>
#include <map>
#include <string>
#include <cstdio>
#include <cmath>
#include <GdiPlus.h>

#ifdef _MSC_VER
#	pragma warning (disable:4996)
#endif

ULONG_PTR gdiplustoken = 0;

GLint map_Format_Internal[14] =
{
	0,
	GL_RGB8,
	GL_RGBA8,
	GL_SRGB8_ALPHA8,
	GL_DEPTH_STENCIL,
	GL_DEPTH_COMPONENT,
	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
	GL_R16F,
	GL_RG16F,
	GL_RGBA16F_ARB,
	GL_R32F,
	GL_RG32F,
	GL_RGBA32F_ARB
};

GLenum map_Format_Format[14] =
{
	0,
	GL_RGB,
	GL_RGBA,
	GL_RGBA,
	GL_DEPTH_STENCIL,
	GL_DEPTH_COMPONENT,
	GL_RGBA,
	GL_RGBA,
	GL_RED,
	GL_RG,
	GL_RGBA,
	GL_RED,
	GL_RG,
	GL_RGBA
};

GLenum map_Format_Type[14] =
{
	0,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_INT_8_8_8_8_REV,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_INT_24_8,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_BYTE,
	GL_FLOAT,
	GL_HALF_FLOAT,
	GL_HALF_FLOAT,
	GL_HALF_FLOAT,
	GL_FLOAT,
	GL_FLOAT,
	GL_FLOAT
};

static void GLReadString(FILE* f, char* buff)
{
	size_t ind = 0;
	char ch = fgetc(f);

	while( ch != '\n' )
	{
		buff[ind] = ch;
		ch = fgetc(f);
		++ind;
	}

	buff[ind] = '\0';
}

static Gdiplus::Bitmap* Win32LoadPicture(const std::wstring& file)
{
	if( gdiplustoken == 0 )
	{
		Gdiplus::GdiplusStartupInput gdiplustartup;
		Gdiplus::GdiplusStartup(&gdiplustoken, &gdiplustartup, NULL);
	}

	Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(file.c_str(), FALSE);

	if( bitmap->GetLastStatus() != Gdiplus::Ok ) {
		delete bitmap;
		bitmap = 0;
	}

	return bitmap;
}

// *****************************************************************************************************************************
//
// OpenGLMesh impl
//
// *****************************************************************************************************************************

OpenGLMesh::OpenGLMesh()
{
	numsubsets		= 0;
	numvertices		= 0;
	numindices		= 0;
	subsettable		= 0;
	vertexbuffer	= 0;
	indexbuffer		= 0;
	vertexlayout	= 0;
	meshoptions		= 0;

	vertexdata_locked.ptr = 0;
	indexdata_locked.ptr = 0;
}

OpenGLMesh::~OpenGLMesh()
{
	Destroy();
}

void OpenGLMesh::Destroy()
{
	if( vertexbuffer )
	{
		glDeleteBuffers(1, &vertexbuffer);
		vertexbuffer = 0;
	}

	if( indexbuffer )
	{
		glDeleteBuffers(1, &indexbuffer);
		indexbuffer = 0;
	}

	if( vertexlayout )
	{
		glDeleteVertexArrays(1, &vertexlayout);
		vertexlayout = 0;
	}

	if( subsettable )
	{
		free(subsettable);
		subsettable = 0;
	}

	numsubsets = 0;
}

bool OpenGLMesh::LockVertexBuffer(GLuint offset, GLuint size, GLuint flags, void** data)
{
	if( offset >= numvertices * vertexdecl.Stride )
	{
		(*data) = 0;
		return false;
	}

	if( size == 0 )
		size = numvertices * vertexdecl.Stride - offset;

	if( flags == 0 )
		flags = GL_MAP_READ_BIT|GL_MAP_WRITE_BIT;

	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	vertexdata_locked.ptr = glMapBufferRange(GL_ARRAY_BUFFER, offset, size, flags);
	vertexdata_locked.flags = flags;

	if( !vertexdata_locked.ptr )
		return false;

	(*data) = vertexdata_locked.ptr;
	return true;
}

bool OpenGLMesh::LockIndexBuffer(GLuint offset, GLuint size, GLuint flags, void** data)
{
	size_t istride = ((meshoptions & GLMESH_32BIT) ? 4 : 2);

	if( offset >= numindices * istride )
	{
		(*data) = 0;
		return false;
	}

	if( size == 0 )
		size = numindices * istride - offset;

	if( flags == 0 )
		flags = GL_MAP_READ_BIT|GL_MAP_WRITE_BIT;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexbuffer);
	indexdata_locked.ptr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, flags);
	indexdata_locked.flags = flags;

	if( !indexdata_locked.ptr )
		return false;

	(*data) = indexdata_locked.ptr;
	return true;
}

void OpenGLMesh::DrawSubset(GLuint subset)
{
	if( vertexlayout == 0 || numvertices == 0 )
		return;

	if( subsettable && subset < numsubsets )
	{
		const OpenGLAttributeRange& attr = subsettable[subset];
		GLenum itype = (meshoptions & GLMESH_32BIT) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
		GLuint start = attr.IndexStart * ((meshoptions & GLMESH_32BIT) ? 4 : 2);

		glBindVertexArray(vertexlayout);

		if( attr.IndexCount == 0 )
			glDrawArrays(attr.PrimitiveType, attr.VertexStart, attr.VertexCount);
		else
		{
			if( attr.VertexCount == 0 )
				glDrawElements(attr.PrimitiveType, attr.IndexCount, itype, (char*)0 + start);
			else
				glDrawRangeElements(attr.PrimitiveType, attr.VertexStart, attr.VertexStart + attr.VertexCount - 1, attr.IndexCount, itype, (char*)0 + start);
		}
	}
}

void OpenGLMesh::ReorderSubsets(GLuint newindices[])
{
	OpenGLAttributeRange tmp;

	for( GLuint i = 0; i < numsubsets; ++i )
	{
		tmp = subsettable[i];
		subsettable[i] = subsettable[newindices[i]];
		subsettable[newindices[i]] = tmp;

		for( GLuint j = i; j < numsubsets; ++j )
		{
			if( newindices[j] == i )
			{
				newindices[j] = newindices[i];
				break;
			}
		}
	}
}

void OpenGLMesh::UnlockVertexBuffer()
{
	if( vertexdata_locked.ptr && vertexbuffer != 0 )
	{
		glUnmapBuffer(GL_ARRAY_BUFFER);
		vertexdata_locked.ptr = 0;
	}
}

void OpenGLMesh::UnlockIndexBuffer()
{
	if( indexdata_locked.ptr && indexbuffer != 0 )
	{
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		indexdata_locked.ptr = 0;
	}
}

void OpenGLMesh::SetAttributeTable(const OpenGLAttributeRange* table, GLuint size)
{
	if( subsettable )
		free(subsettable);

	subsettable = (OpenGLAttributeRange*)malloc(size * sizeof(OpenGLAttributeRange));
	memcpy(subsettable, table, size * sizeof(OpenGLAttributeRange));

	numsubsets = size;
}

// *****************************************************************************************************************************
//
// OpenGLEffect impl
//
// *****************************************************************************************************************************

OpenGLEffect::OpenGLEffect()
{
	floatvalues	= 0;
	intvalues	= 0;
	floatcap	= 0;
	floatsize	= 0;
	intcap		= 0;
	intsize		= 0;
	program		= 0;
}

OpenGLEffect::~OpenGLEffect()
{
	Destroy();
}

void OpenGLEffect::Destroy()
{
	if( floatvalues )
		delete[] floatvalues;

	if( intvalues )
		delete[] intvalues;

	floatvalues = 0;
	intvalues = 0;

	if( program )
		glDeleteProgram(program);

	program = 0;
}

void OpenGLEffect::AddUniform(const char* name, GLuint location, GLuint count, GLenum type)
{
	Uniform uni;

	if( strlen(name) >= sizeof(uni.Name) )
		throw 1;

	strcpy(uni.Name, name);

	if( type == GL_FLOAT_MAT4 )
		count = 4;

	uni.Type = type;
	uni.RegisterCount = count;
	uni.Location = location;
	uni.Changed = true;

	if( type == GL_FLOAT || (type >= GL_FLOAT_VEC2 && type <= GL_FLOAT_VEC4) || type == GL_FLOAT_MAT4 )
	{
		uni.StartRegister = floatsize;

		if( floatsize + count > floatcap )
		{
			unsigned int newcap = max(floatsize + count, floatsize + 8);

			floatvalues = (float*)realloc(floatvalues, newcap * 4 * sizeof(float));
			floatcap = newcap;
		}

		float* reg = (floatvalues + uni.StartRegister * 4);

		if( type == GL_FLOAT_MAT4 )
			GLMatrixIdentity(reg);
		else
			memset(reg, 0, uni.RegisterCount * 4 * sizeof(float));

		floatsize += count;
	}
	else if( type == GL_INT || (type >= GL_INT_VEC2 && type <= GL_INT_VEC4) || type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE || type == GL_IMAGE_2D )
	{
		uni.StartRegister = intsize;

		if( intsize + count > intcap )
		{
			unsigned int newcap = max(intsize + count, intsize + 8);

			intvalues = (int*)realloc(intvalues, newcap * 4 * sizeof(int));
			intcap = newcap;
		}

		int* reg = (intvalues + uni.StartRegister * 4);
		memset(reg, 0, uni.RegisterCount * 4 * sizeof(int));

		intsize += count;
	}
	else
		// not handled
		throw 1;

	uniforms.insert(uni);
}

void OpenGLEffect::BindAttributes()
{
	typedef std::map<std::string, GLuint> semanticmap;

	if( program == 0 )
		return;

	semanticmap	attribmap;
	GLint		count;
	GLenum		type;
	GLint		size, loc;
	GLchar		attribname[256];

	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);

	attribmap["my_Position"]	= GLDECLUSAGE_POSITION;
	attribmap["my_Normal"]		= GLDECLUSAGE_NORMAL;
	attribmap["my_Tangent"]		= GLDECLUSAGE_TANGENT;
	attribmap["my_Binormal"]	= GLDECLUSAGE_BINORMAL;
	attribmap["my_Color"]		= GLDECLUSAGE_COLOR;
	attribmap["my_Texcoord0"]	= GLDECLUSAGE_TEXCOORD;
	attribmap["my_Texcoord1"]	= GLDECLUSAGE_TEXCOORD + 10;
	attribmap["my_Texcoord2"]	= GLDECLUSAGE_TEXCOORD + 11;
	attribmap["my_Texcoord3"]	= GLDECLUSAGE_TEXCOORD + 12;
	attribmap["my_Texcoord4"]	= GLDECLUSAGE_TEXCOORD + 13;
	attribmap["my_Texcoord5"]	= GLDECLUSAGE_TEXCOORD + 14;
	attribmap["my_Texcoord6"]	= GLDECLUSAGE_TEXCOORD + 15;
	attribmap["my_Texcoord7"]	= GLDECLUSAGE_TEXCOORD + 16;

	for( int i = 0; i < count; ++i )
	{
		memset(attribname, 0, sizeof(attribname));

		glGetActiveAttrib(program, i, 256, NULL, &size, &type, attribname);
		loc = glGetAttribLocation(program, attribname);

		semanticmap::iterator it = attribmap.find(qstring(attribname));

		if( loc == -1 || it == attribmap.end() )
		{
			std::cout <<
				"Invalid attribute found. Use the my_<semantic> syntax!\n";
		}
		else
			glBindAttribLocation(program, it->second, attribname);
	}

	// bind fragment shader output
	GLint numrts;

	glGetIntegerv(GL_MAX_DRAW_BUFFERS, &numrts);
	glBindFragDataLocation(program, 0, "my_FragColor0");

	if( numrts > 1 )
		glBindFragDataLocation(program, 1, "my_FragColor1");

	if( numrts > 2 )
		glBindFragDataLocation(program, 2, "my_FragColor2");
		
	if( numrts > 3 )
		glBindFragDataLocation(program, 3, "my_FragColor3");

	// relink
	glLinkProgram(program);
}

void OpenGLEffect::QueryUniforms()
{
	GLint		count;
	GLenum		type;
	GLsizei		length;
	GLint		size, loc;
	GLchar		uniname[256];

	if( program == 0 )
		return;

	memset(uniname, 0, sizeof(uniname));
	uniforms.clear();

	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

	// uniforms
	for( int i = 0; i < count; ++i )
	{
		memset(uniname, 0, sizeof(uniname));

		glGetActiveUniform(program, i, 256, &length, &size, &type, uniname);
		loc = glGetUniformLocation(program, uniname);

		for( int j = 0; j < length; ++j )
		{
			if( uniname[j] == '[' )
				uniname[j] = '\0';
		}

		if( loc == -1 )
			continue;

		AddUniform(uniname, loc, size, type);
	}
}

void OpenGLEffect::Begin()
{
	glUseProgram(program);
	CommitChanges();
}

void OpenGLEffect::CommitChanges()
{
	for( size_t i = 0; i < uniforms.size(); ++i )
	{
		const Uniform& uni = uniforms[i];
		float* floatdata = (floatvalues + uni.StartRegister * 4);
		int* intdata = (intvalues + uni.StartRegister * 4);

		if( !uni.Changed )
			continue;

		uni.Changed = false;

		switch( uni.Type )
		{
		case GL_FLOAT:
			glUniform1fv(uni.Location, uni.RegisterCount, floatdata);
			break;

		case GL_FLOAT_VEC2:
			glUniform2fv(uni.Location, uni.RegisterCount, floatdata);
			break;

		case GL_FLOAT_VEC3:
			glUniform3fv(uni.Location, uni.RegisterCount, floatdata);
			break;

		case GL_FLOAT_VEC4:
			glUniform4fv(uni.Location, uni.RegisterCount, floatdata);
			break;

		case GL_FLOAT_MAT4:
			glUniformMatrix4fv(uni.Location, uni.RegisterCount / 4, false, floatdata);
			break;

		case GL_INT:
		case GL_SAMPLER_2D:
		case GL_SAMPLER_CUBE:
			glUniform1i(uni.Location, intdata[0]);
			break;

		default:
			break;
		}
	}
}

void OpenGLEffect::End()
{
	// do nothing
}

void OpenGLEffect::SetMatrix(const char* name, const float* value)
{
	SetVector(name, value);
}

void OpenGLEffect::SetVector(const char* name, const float* value)
{
	Uniform test;
	strcpy(test.Name, name);

	size_t id = uniforms.find(test);

	if( id < uniforms.size() )
	{
		const Uniform& uni = uniforms[id];
		float* reg = (floatvalues + uni.StartRegister * 4);

		memcpy(reg, value, uni.RegisterCount * 4 * sizeof(float));
		uni.Changed = true;
	}
}

void OpenGLEffect::SetVectorArray(const char* name, const float* values, GLsizei count)
{
	Uniform test;
	strcpy(test.Name, name);

	size_t id = uniforms.find(test);

	if( id < uniforms.size() )
	{
		const Uniform& uni = uniforms[id];
		float* reg = (floatvalues + uni.StartRegister * 4);

		if( count > uni.RegisterCount )
			count = uni.RegisterCount;

		memcpy(reg, values, count * 4 * sizeof(float));
		uni.Changed = true;
	}
}

void OpenGLEffect::SetFloat(const char* name, float value)
{
	Uniform test;
	strcpy(test.Name, name);

	size_t id = uniforms.find(test);

	if( id < uniforms.size() )
	{
		const Uniform& uni = uniforms[id];
		float* reg = (floatvalues + uni.StartRegister * 4);

		reg[0] = value;
		uni.Changed = true;
	}
}

void OpenGLEffect::SetFloatArray(const char* name, const float* values, GLsizei count)
{
	Uniform test;
	strcpy(test.Name, name);

	size_t id = uniforms.find(test);

	if( id < uniforms.size() )
	{
		const Uniform& uni = uniforms[id];
		float* reg = (floatvalues + uni.StartRegister * 4);

		if( count > uni.RegisterCount )
			count = uni.RegisterCount;

		memcpy(reg, values, count * sizeof(float));
		uni.Changed = true;
	}
}

void OpenGLEffect::SetInt(const char* name, int value)
{
	Uniform test;
	strcpy(test.Name, name);

	size_t id = uniforms.find(test);

	if( id < uniforms.size() )
	{
		const Uniform& uni = uniforms[id];
		int* reg = (intvalues + uni.StartRegister * 4);

		reg[0] = value;
		uni.Changed = true;
	}
}

// *****************************************************************************************************************************
//
// OpenGLFrameBuffer impl
//
// *****************************************************************************************************************************

OpenGLFramebuffer::OpenGLFramebuffer(GLuint width, GLuint height)
{
	glGenFramebuffers(1, &fboid);

	sizex = width;
	sizey = height;
}

OpenGLFramebuffer::~OpenGLFramebuffer()
{
	for( int i = 0; i < 8; ++i )
	{
		if( rendertargets[i].id != 0 )
		{
			if( rendertargets[i].type == 0 )
				glDeleteRenderbuffers(1, &rendertargets[i].id);
			else
				glDeleteTextures(1, &rendertargets[i].id);
		}
	}

	if( depthstencil.id != 0 )
	{
		if( depthstencil.type == 0 )
			glDeleteRenderbuffers(1, &depthstencil.id);
		else
			glDeleteTextures(1, &depthstencil.id);
	}

	glDeleteFramebuffers(1, &fboid);
}

bool OpenGLFramebuffer::AttachRenderbuffer(GLenum target, OpenGLFormat format, GLsizei samples)
{
	Attachment* attach = 0;

	if( target == GL_DEPTH_ATTACHMENT || target == GL_DEPTH_STENCIL_ATTACHMENT )
		attach = &depthstencil;
	else if( target >= GL_COLOR_ATTACHMENT0 && target < GL_COLOR_ATTACHMENT8 )
		attach = &rendertargets[target - GL_COLOR_ATTACHMENT0];
	else
	{
		std::cout << "Target is invalid!\n";
		return false;
	}

	if( attach->id != 0 )
	{
		std::cout << "Already attached to this target!\n";
		return false;
	}

	glGenRenderbuffers(1, &attach->id);
	
	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glBindRenderbuffer(GL_RENDERBUFFER, attach->id);

	if( samples > 1 )
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, map_Format_Internal[format], sizex, sizey);
	else
		glRenderbufferStorage(GL_RENDERBUFFER, map_Format_Internal[format], sizex, sizey);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, target, GL_RENDERBUFFER, attach->id);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	attach->type = 0;

	return true;
}

bool OpenGLFramebuffer::AttachCubeTexture(GLenum target, OpenGLFormat format, GLenum filter)
{
	Attachment* attach = 0;

	if( target == GL_DEPTH_ATTACHMENT || target == GL_DEPTH_STENCIL_ATTACHMENT )
		attach = &depthstencil;
	else if( target >= GL_COLOR_ATTACHMENT0 && target < GL_COLOR_ATTACHMENT8 )
		attach = &rendertargets[target - GL_COLOR_ATTACHMENT0];
	else
	{
		std::cout << "Target is invalid!\n";
		return false;
	}

	if( attach->id != 0 )
	{
		std::cout << "Already attached to this target!\n";
		return false;
	}

	glGenTextures(1, &attach->id);

	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glBindTexture(GL_TEXTURE_CUBE_MAP, attach->id);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, map_Format_Internal[format], sizex, sizex, 0,
		map_Format_Format[format], map_Format_Type[format], NULL);

	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_CUBE_MAP_POSITIVE_X, attach->id, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	attach->type = 2;

	return true;
}

bool OpenGLFramebuffer::AttachTexture(GLenum target, OpenGLFormat format, GLenum filter)
{
	Attachment* attach = 0;

	if( target == GL_DEPTH_ATTACHMENT || target == GL_DEPTH_STENCIL_ATTACHMENT )
		attach = &depthstencil;
	else if( target >= GL_COLOR_ATTACHMENT0 && target < GL_COLOR_ATTACHMENT8 )
		attach = &rendertargets[target - GL_COLOR_ATTACHMENT0];
	else
	{
		std::cout << "Target is invalid!\n";
		return false;
	}

	if( attach->id != 0 )
	{
		std::cout << "Already attached to this target!\n";
		return false;
	}

	glGenTextures(1, &attach->id);

	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glBindTexture(GL_TEXTURE_2D, attach->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glTexImage2D(GL_TEXTURE_2D, 0, map_Format_Internal[format], sizex, sizey, 0, map_Format_Format[format], map_Format_Type[format], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_2D, attach->id, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	attach->type = 1;

	return true;
}

bool OpenGLFramebuffer::Validate()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fboid);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	switch( status )
	{
	case GL_FRAMEBUFFER_COMPLETE:
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		std::cout << "OpenGLFramebuffer::Validate(): GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!\n";
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
		std::cout << "OpenGLFramebuffer::Validate(): GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS!\n";
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		std::cout << "OpenGLFramebuffer::Validate(): GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!\n";
		break;

	default:
		std::cout << "OpenGLFramebuffer::Validate(): Unknown error!\n";
		break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return (status == GL_FRAMEBUFFER_COMPLETE);
}

void OpenGLFramebuffer::Detach(GLenum target)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_2D, 0, 0);
}

void OpenGLFramebuffer::Reattach(GLenum target, GLint level)
{
	Attachment* attach = 0;

	if( target == GL_DEPTH_ATTACHMENT || target == GL_DEPTH_STENCIL_ATTACHMENT )
		attach = &depthstencil;
	else
		attach = &rendertargets[target - GL_COLOR_ATTACHMENT0];

	if( attach->type != 1 )
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_2D, attach->id, level);
}

void OpenGLFramebuffer::Reattach(GLenum target, GLint face, GLint level)
{
	Attachment* attach = 0;

	if( target == GL_DEPTH_ATTACHMENT || target == GL_DEPTH_STENCIL_ATTACHMENT )
		attach = &depthstencil;
	else
		attach = &rendertargets[target - GL_COLOR_ATTACHMENT0];

	if( attach->type != 2 )
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, attach->id, level);
}

void OpenGLFramebuffer::Resolve(OpenGLFramebuffer* to, GLbitfield mask)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fboid);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to->fboid);

	GLenum filter = ((mask & (GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT)) ? GL_NEAREST : GL_LINEAR);
	glBlitFramebuffer(0, 0, sizex, sizey, 0, 0, to->sizex, to->sizey, mask, filter);
}

void OpenGLFramebuffer::Set()
{
	GLenum buffs[8];
	GLsizei count = 0;

	for( int i = 0; i < 8; ++i )
	{
		if( rendertargets[i].id != 0 )
		{
			buffs[i] = GL_COLOR_ATTACHMENT0 + i;
			count = i;
		}
		else
			buffs[i] = GL_NONE;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fboid);

	if( count > 0 )
		glDrawBuffers(count + 1, buffs);

	glViewport(0, 0, sizex, sizey);
}

void OpenGLFramebuffer::Unset()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
}

// *****************************************************************************************************************************
//
// OpenGLScreenQuad impl
//
// *****************************************************************************************************************************

OpenGLScreenQuad::OpenGLScreenQuad()
{
	vertexbuffer = 0;
	vertexlayout = 0;

	glGenBuffers(1, &vertexbuffer);
	glGenVertexArrays(1, &vertexlayout);

	float vertices[] =
	{
		-1, -1, 0, 0, 0,
		1, -1, 0, 1, 0,
		-1, 1, 0, 0, 1,

		-1, 1, 0, 0, 1,
		1, -1, 0, 1, 0,
		1, 1, 0, 1, 1
	};

	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, 6 * 5 * sizeof(float), vertices, GL_STATIC_DRAW);

	glBindVertexArray(vertexlayout);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);

		glEnableVertexAttribArray(GLDECLUSAGE_POSITION);
		glVertexAttribPointer(GLDECLUSAGE_POSITION, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(GLDECLUSAGE_TEXCOORD);
		glVertexAttribPointer(GLDECLUSAGE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)12);
	}
	glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

OpenGLScreenQuad::~OpenGLScreenQuad()
{
	if( vertexlayout )
		glDeleteVertexArrays(1, &vertexlayout);

	if( vertexbuffer )
		glDeleteBuffers(1, &vertexbuffer);
}

void OpenGLScreenQuad::Draw()
{
	glBindVertexArray(vertexlayout);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

// *****************************************************************************************************************************
//
// Functions impl
//
// *****************************************************************************************************************************

bool GLCreateMesh(GLuint numvertices, GLuint numindices, GLuint options, OpenGLVertexElement* decl, OpenGLMesh** mesh)
{
	OpenGLMesh* glmesh = new OpenGLMesh();

	glGenBuffers(1, &glmesh->vertexbuffer);
	glGenBuffers(1, &glmesh->indexbuffer);
	glGenVertexArrays(1, &glmesh->vertexlayout);

	if( numvertices >= 0xffff )
		options |= GLMESH_32BIT;

	glmesh->meshoptions					= options;
	glmesh->numsubsets					= 1;
	glmesh->numvertices					= numvertices;
	glmesh->numindices					= numindices;
	glmesh->subsettable					= (OpenGLAttributeRange*)malloc(sizeof(OpenGLAttributeRange));

	glmesh->subsettable->AttribId		= 0;
	glmesh->subsettable->IndexCount		= numindices;
	glmesh->subsettable->IndexStart		= 0;
	glmesh->subsettable->PrimitiveType	= GLPT_TRIANGLELIST;
	glmesh->subsettable->VertexCount	= 0;	// draw entire buffer
	glmesh->subsettable->VertexStart	= 0;

	glmesh->vertexdecl.Stride = 0;

	// calculate stride
	for( int i = 0; i < 16; ++i )
	{
		OpenGLVertexElement& elem = decl[i];

		if( elem.Stream == 0xff )
			break;

		switch( elem.Type )
		{
		case GLDECLTYPE_GLCOLOR:
		case GLDECLTYPE_FLOAT1:		glmesh->vertexdecl.Stride += 4;		break;
		case GLDECLTYPE_FLOAT2:		glmesh->vertexdecl.Stride += 8;		break;
		case GLDECLTYPE_FLOAT3:		glmesh->vertexdecl.Stride += 12;	break;
		case GLDECLTYPE_FLOAT4:		glmesh->vertexdecl.Stride += 16;	break;

		default:
			break;
		}
	}

	// allocate storage
	GLenum usage = ((options & GLMESH_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	GLuint istride = ((options & GLMESH_32BIT) ? 4 : 2);

	glBindBuffer(GL_ARRAY_BUFFER, glmesh->vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, numvertices * glmesh->vertexdecl.Stride, 0, usage);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glmesh->indexbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numindices * istride, 0, usage);

	// create vertex layout
	glBindVertexArray(glmesh->vertexlayout);
	{
		glBindBuffer(GL_ARRAY_BUFFER, glmesh->vertexbuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glmesh->indexbuffer);

		// bind locations
		for( int i = 0; i < 16; ++i )
		{
			OpenGLVertexElement& elem = decl[i];

			if( elem.Stream == 0xff )
				break;

			glEnableVertexAttribArray(elem.Usage);

			switch( elem.Usage )
			{
			case GLDECLUSAGE_POSITION:
				glVertexAttribPointer(elem.Usage, (elem.Type == GLDECLTYPE_FLOAT4 ? 4 : 3), GL_FLOAT, GL_FALSE, glmesh->vertexdecl.Stride, (const GLvoid*)elem.Offset);
				break;

			case GLDECLUSAGE_COLOR:
				glVertexAttribPointer(elem.Usage, 4, GL_UNSIGNED_BYTE, GL_TRUE, glmesh->vertexdecl.Stride, (const GLvoid*)elem.Offset);
				break;

			case GLDECLUSAGE_NORMAL:
				glVertexAttribPointer(elem.Usage, (elem.Type == GLDECLTYPE_FLOAT4 ? 4 : 3), GL_FLOAT, GL_FALSE, glmesh->vertexdecl.Stride, (const GLvoid*)elem.Offset);
				break;

			case GLDECLUSAGE_TEXCOORD:
				// haaack...
				glVertexAttribPointer(elem.Usage + elem.UsageIndex, (elem.Type + 1), GL_FLOAT, GL_FALSE, glmesh->vertexdecl.Stride, (const GLvoid*)elem.Offset);
				break;

			// TODO:

			default:
				std::cout << "Unhandled layout element...\n";
				break;
			}
		}
	}
	glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	(*mesh) = glmesh;
	return true;
}

bool GLCreateMeshFromQM(const char* file, OpenGLMaterial** materials, GLuint* nummaterials, OpenGLMesh** mesh)
{
	static const unsigned char usages[] =
	{
		GLDECLUSAGE_POSITION,
		GLDECLUSAGE_POSITIONT,
		GLDECLUSAGE_COLOR,
		GLDECLUSAGE_BLENDWEIGHT,
		GLDECLUSAGE_BLENDINDICES,
		GLDECLUSAGE_NORMAL,
		GLDECLUSAGE_TEXCOORD,
		GLDECLUSAGE_TANGENT,
		GLDECLUSAGE_BINORMAL,
		GLDECLUSAGE_PSIZE,
		GLDECLUSAGE_TESSFACTOR
	};

	static const unsigned short elemsizes[6] =
	{
		1, 2, 3, 4, 4, 4
	};

	static const unsigned short elemstrides[6] =
	{
		4, 4, 4, 4, 1, 1
	};

	OpenGLAABox				box;
	OpenGLVertexElement*	decl;
	OpenGLAttributeRange*	table;
	OpenGLMaterial			defmat;
	OpenGLMaterial*			mat;
	OpenGLColor				color;

	std::string				str;
	FILE*					infile = 0;
	float					bbmin[3];
	float					bbmax[3];
	unsigned int			unused;
	unsigned int			version;
	unsigned int			numindices;
	unsigned int			numvertices;
	unsigned int			vstride;
	unsigned int			istride;
	unsigned int			numsubsets;
	unsigned int			numelems;
	unsigned short			tmp16;
	unsigned char			tmp8;
	void*					data;
	char					buff[256];
	bool					success;

#ifdef _MSC_VER
	fopen_s(&infile, file, "rb");
#else
	infile = fopen(file, "rb");
#endif

	if( !infile )
		return false;

	fread(&unused, 4, 1, infile);
	fread(&numindices, 4, 1, infile);
	fread(&istride, 4, 1, infile);
	fread(&numsubsets, 4, 1, infile);

	version = unused >> 16;

	fread(&numvertices, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);

	table = new OpenGLAttributeRange[numsubsets];

	// vertex declaration
	fread(&numelems, 4, 1, infile);
	decl = new OpenGLVertexElement[numelems + 1];

	vstride = 0;

	for( unsigned int i = 0; i < numelems; ++i )
	{
		fread(&tmp16, 2, 1, infile);
		decl[i].Stream = tmp16;

		fread(&tmp8, 1, 1, infile);
		decl[i].Usage = usages[tmp8];

		fread(&tmp8, 1, 1, infile);
		decl[i].Type = tmp8;

		fread(&tmp8, 1, 1, infile);
		decl[i].UsageIndex = tmp8;
		decl[i].Offset = vstride;

		vstride += elemsizes[decl[i].Type] * elemstrides[decl[i].Type];
	}

	decl[numelems].Stream = 0xff;
	decl[numelems].Offset = 0;
	decl[numelems].Type = 0;
	decl[numelems].Usage = 0;
	decl[numelems].UsageIndex = 0;

	// create mesh
	success = GLCreateMesh(numvertices, numindices, 0, decl, mesh);

	if( !success )
		goto _fail;

	(*mesh)->LockVertexBuffer(0, 0, GLLOCK_DISCARD, &data);
	fread(data, vstride, numvertices, infile);
	(*mesh)->UnlockVertexBuffer();

	(*mesh)->LockIndexBuffer(0, 0, GLLOCK_DISCARD, &data);
	fread(data, istride, numindices, infile);
	(*mesh)->UnlockIndexBuffer();

	if( version >= 1 )
	{
		fread(&unused, 4, 1, infile);

		if( unused > 0 )
			fseek(infile, 8 * unused, SEEK_CUR);
	}

	// attribute table
	if( materials )
		(*materials) = new OpenGLMaterial[numsubsets];

	for( unsigned int i = 0; i < numsubsets; ++i )
	{
		OpenGLAttributeRange& subset = table[i];

		if( materials )
			mat = ((*materials) + i);
		else
			mat = &defmat;

		mat->TextureFile = 0;

		subset.AttribId = i;
		subset.PrimitiveType = GLPT_TRIANGLELIST;

		fread(&subset.IndexStart, 4, 1, infile);
		fread(&subset.VertexStart, 4, 1, infile);
		fread(&subset.VertexCount, 4, 1, infile);
		fread(&subset.IndexCount, 4, 1, infile);

		fread(bbmin, sizeof(float), 3, infile);
		fread(bbmax, sizeof(float), 3, infile);

		box.Add(bbmin);
		box.Add(bbmax);

		(*mesh)->boundingbox.Add(bbmin);
		(*mesh)->boundingbox.Add(bbmax);

		GLReadString(infile, buff);
		GLReadString(infile, buff);

		bool hasmaterial = (buff[1] != ',');

		if( hasmaterial )
		{
			fread(&color, sizeof(OpenGLColor), 1, infile);
			mat->Ambient = color;

			fread(&color, sizeof(OpenGLColor), 1, infile);
			mat->Diffuse = color;

			fread(&color, sizeof(OpenGLColor), 1, infile);
			mat->Specular = color;

			fread(&color, sizeof(OpenGLColor), 1, infile);
			mat->Emissive = color;

			if( version >= 2 )
				fseek(infile, 16, SEEK_CUR);	// uvscale

			fread(&mat->Power, sizeof(float), 1, infile);
			fread(&mat->Diffuse.a, sizeof(float), 1, infile);

			fread(&unused, 4, 1, infile);
			GLReadString(infile, buff);

			if( buff[1] != ',' )
			{
				unused = strlen(buff);

				if( materials )
				{
					mat->TextureFile = new char[unused + 1];
					memcpy(mat->TextureFile, buff, unused);
					mat->TextureFile[unused] = 0;
				}
			}

			GLReadString(infile, buff);
			GLReadString(infile, buff);
			GLReadString(infile, buff);
			GLReadString(infile, buff);
			GLReadString(infile, buff);
			GLReadString(infile, buff);
			GLReadString(infile, buff);
		}
		else
		{
			color = OpenGLColor(1, 1, 1, 1);

			memcpy(&mat->Diffuse, &color, 4 * sizeof(float));
			memcpy(&mat->Specular, &color, 4 * sizeof(float));

			color = OpenGLColor(0, 0, 0, 1);

			memcpy(&mat->Emissive, &color, 4 * sizeof(float));
			memcpy(&mat->Ambient, &color, 4 * sizeof(float));

			mat->Power = 80.0f;
		}

		GLReadString(infile, buff);

		if( buff[1] != ',' && mat->TextureFile == 0 )
		{
			unused = strlen(buff);

			mat->TextureFile = new char[unused + 1];
			memcpy(mat->TextureFile, buff, unused);
			mat->TextureFile[unused] = 0;
		}

		GLReadString(infile, buff);
		GLReadString(infile, buff);
		GLReadString(infile, buff);
		GLReadString(infile, buff);
		GLReadString(infile, buff);
		GLReadString(infile, buff);
		GLReadString(infile, buff);
	}

	// attribute buffer
	(*mesh)->SetAttributeTable(table, numsubsets);

	if( nummaterials )
		*nummaterials = numsubsets;

	// printf some info
	GLGetFile(str, file);
	box.GetSize(bbmin);

	printf("Loaded mesh '%s': size = (%.3f, %.3f, %.3f)\n", str.c_str(), bbmin[0], bbmin[1], bbmin[2]);

_fail:
	delete[] decl;
	delete[] table;

	fclose(infile);
	return success;
}

bool GLCreatePlane(float width, float height, float uscale, float vscale, OpenGLMesh** mesh)
{
	OpenGLVertexElement decl[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0, 12, GLDECLTYPE_FLOAT3, GLDECLUSAGE_NORMAL, 0 },
		{ 0, 24, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	OpenGLAttributeRange table[] =
	{
		{ GLPT_TRIANGLELIST, 0, 0, 6, 0, 4 }
	};

	if( !GLCreateMesh(4, 6, 0, decl, mesh) )
		return false;

	OpenGLAABox box;
	float* vdata = 0;
	GLushort* idata = 0;

	(*mesh)->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	(*mesh)->LockIndexBuffer(0, 0, GLLOCK_DISCARD, (void**)&idata);

	vdata[0] = width * -0.5f;	vdata[8] = width * 0.5f;	vdata[16] = width * 0.5f;	vdata[24] = width * -0.5f;
	vdata[1] = height * -0.5f;	vdata[9] = height * -0.5f;	vdata[17] = height * 0.5f;	vdata[25] = height * 0.5f;
	vdata[2] = 0;				vdata[10] = 0;				vdata[18] = 0;				vdata[26] = 0;

	vdata[3] = 0;				vdata[11] = 0;				vdata[19] = 0;				vdata[27] = 0;
	vdata[4] = 0;				vdata[12] = 0;				vdata[20] = 0;				vdata[28] = 0;
	vdata[5] = 1;				vdata[13] = 1;				vdata[21] = 1;				vdata[29] = 1;

	vdata[6] = 0;				vdata[14] = uscale;			vdata[22] = uscale;			vdata[30] = 0;
	vdata[7] = 0;				vdata[15] = 0;				vdata[23] = vscale;			vdata[31] = vscale;

	idata[0] = 0;	idata[3] = 0;
	idata[1] = 1;	idata[4] = 2;
	idata[2] = 2;	idata[5] = 3;

	box.Min[0] = width * -0.5f;		box.Max[0] = width * 0.5f;
	box.Min[1] = height * -0.5f;	box.Max[1] = height * 0.5f;
	box.Min[2] = -1e-3f;			box.Max[2] = 1e-3f;

	(*mesh)->UnlockIndexBuffer();
	(*mesh)->UnlockVertexBuffer();

	(*mesh)->SetAttributeTable(table, 1);
	(*mesh)->SetBoundingBox(box);

	return true;
}

bool GLCreateBox(float width, float height, float depth, float uscale, float vscale, float wscale, OpenGLMesh** mesh)
{
	OpenGLVertexElement decl[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0, 12, GLDECLTYPE_FLOAT3, GLDECLUSAGE_NORMAL, 0 },
		{ 0, 24, GLDECLTYPE_FLOAT2, GLDECLUSAGE_TEXCOORD, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	OpenGLAttributeRange table[] =
	{
		{ GLPT_TRIANGLELIST, 0, 0, 36, 0, 24 }
	};

	if( !GLCreateMesh(24, 36, 0, decl, mesh) )
		return false;

	OpenGLAABox box;
	float* vdata = 0;
	GLushort* idata = 0;

	(*mesh)->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	(*mesh)->LockIndexBuffer(0, 0, GLLOCK_DISCARD, (void**)&idata);

	float v0[3] = { -0.5f, -0.5f, 0.5f };
	float v1[3] = { 0.5f, -0.5f, 0.5f };
	float v2[3] = { 0.5f, 0.5f, 0.5f };
	float v3[3] = { -0.5f, 0.5f, 0.5f };

	float rotmatrices[6][16] =
	{
		{ width, 0, 0, 0, 0, height, 0, 0, 0, 0, depth, 0, 0, 0, 0, 1 }, // front
		{ -width, 0, 0, 0, 0, height, 0, 0, 0, 0, -depth, 0, 0, 0, 0, 1 }, // back
		{ 0, 0, depth, 0, 0, height, 0, 0, -width, 0, 0, 0, 0, 0, 0, 1 }, // left
		{ 0, 0, -depth, 0, 0, height, 0, 0, width, 0, 0, 0, 0, 0, 0, 1 }, // right
		{ width, 0, 0, 0, 0, 0, depth, 0, 0, -height, 0, 0, 0, 0, 0, 1 }, // bottom
		{ width, 0, 0, 0, 0, 0, -depth, 0, 0, height, 0, 0, 0, 0, 0, 1 }, // top
	};

	float normals[6][3] =
	{
		{ 0, 0, 1 },
		{ 0, 0, -1 },
		{ -1, 0, 0 },
		{ 1, 0, 0 },
		{ 0, -1, 0 },
		{ 0, 1, 0 }
	};

	float uvs[6][2] =
	{
		{ uscale, vscale },
		{ uscale, vscale },
		{ wscale, vscale },
		{ wscale, vscale },
		{ uscale, wscale },
		{ uscale, wscale },
	};

	for( int i = 0; i < 6; ++i )
	{
		GLVec3TransformCoord(vdata + 0, v0, rotmatrices[i]);
		GLVec3TransformCoord(vdata + 8, v1, rotmatrices[i]);
		GLVec3TransformCoord(vdata + 16, v2, rotmatrices[i]);
		GLVec3TransformCoord(vdata + 24, v3, rotmatrices[i]);

		GLVec3Set(vdata + 3, normals[i][0], normals[i][1], normals[i][2]);
		GLVec3Set(vdata + 11, normals[i][0], normals[i][1], normals[i][2]);
		GLVec3Set(vdata + 19, normals[i][0], normals[i][1], normals[i][2]);
		GLVec3Set(vdata + 27, normals[i][0], normals[i][1], normals[i][2]);

		vdata[6] = 0;	vdata[14] = uvs[i][0];	vdata[22] = uvs[i][0];	vdata[30] = 0;
		vdata[7] = 0;	vdata[15] = 0;			vdata[23] = uvs[i][1];	vdata[31] = uvs[i][1];

		idata[0] = i * 4 + 0;
		idata[1] = i * 4 + 1;
		idata[2] = i * 4 + 2;

		idata[3] = i * 4 + 0;
		idata[4] = i * 4 + 2;
		idata[5] = i * 4 + 3;

		vdata += 32;
		idata += 6;
	}

	box.Min[0] = width * -0.5f;		box.Max[0] = width * 0.5f;
	box.Min[1] = height * -0.5f;	box.Max[1] = height * 0.5f;
	box.Min[2] = depth * -0.5f;		box.Max[2] = depth * 0.5f;

	(*mesh)->UnlockIndexBuffer();
	(*mesh)->UnlockVertexBuffer();

	(*mesh)->SetAttributeTable(table, 1);
	(*mesh)->SetBoundingBox(box);

	return true;
}

bool GLCreateCapsule(float length, float radius, OpenGLMesh** mesh)
{
	OpenGLAABox	box;
	GLushort	segments	= 16;
	GLuint		numvertices	= (segments * segments + 1) * 2;
	GLuint		numindices	= ((2 * segments - 1) * segments * 3) * 2 + segments * 6;

	OpenGLVertexElement decl[] =
	{
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	OpenGLAttributeRange table[] =
	{
		{ GLPT_TRIANGLELIST, 0, 0, numindices, 0, numvertices }
	};

	if( !GLCreateMesh(numvertices, numindices, 0, decl, mesh) )
		return false;

	float		(*vdata)[3] = 0;
	float*		vert1;
	float*		vert2;
	GLushort*	idata = 0;
	GLushort*	inds1;
	GLushort*	inds2;
	GLushort	vnext = numvertices / 2;
	GLushort	inext = (2 * segments - 1) * segments * 3;

	(*mesh)->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	(*mesh)->LockIndexBuffer(0, 0, GLLOCK_DISCARD, (void**)&idata);

	// caps
	float phistep	= GL_2PI / segments;
	float thetastep	= GL_HALF_PI / segments;
	float phi		= 0;
	float theta		= GL_HALF_PI;

	for( GLushort i = 0; i < segments; ++i )
	{
		float costheta = cosf(theta);
		float sintheta = sinf(theta);

		phi = 0;

		for( GLushort j = 0; j < segments; ++j )
		{
			vert1 = vdata[i * segments + j];
			vert2 = vdata[vnext + i * segments + j];

			vert1[0] = length * -0.5f - costheta * radius;
			vert1[1] = sintheta * sinf(phi) * radius;
			vert1[2] = sintheta * cosf(phi) * radius;

			vert2[0] = length * 0.5f + costheta * radius;
			vert2[1] = sintheta * sinf(phi) * radius;
			vert2[2] = sintheta * cosf(phi) * radius;

			phi += phistep;
			
			GLushort start1 = i * segments;
			GLushort start2 = (i + 1) * segments;
			GLushort wrap = (j + 1) % segments;

			if( i < segments - 1 )
			{
				inds1 = (idata + i * segments * 6 + j * 6);
				inds2 = (idata + inext + i * segments * 6 + j * 6);

				inds1[0] = start1 + j;				inds1[3] = start1 + wrap;
				inds1[1] = start1 + wrap;			inds1[4] = start2 + wrap;
				inds1[2] = start2 + j;				inds1[5] = start2 + j;

				inds2[0] = vnext + start1 + j;		inds2[3] = vnext + start1 + wrap;
				inds2[1] = vnext + start2 + j;		inds2[4] = vnext + start2 + j;
				inds2[2] = vnext + start1 + wrap;	inds2[5] = vnext + start2 + wrap;
			}
		}

		theta -= thetastep;
	}

	vert1 = vdata[segments * segments];
	vert2 = vdata[vnext + segments * segments];

	vert1[0] = length * -0.5f - radius;
	vert1[1] = 0;
	vert1[2] = 0;

	vert2[0] = length * 0.5f + radius;
	vert2[1] = 0;
	vert2[2] = 0;

	for( GLushort j = 0; j < segments; ++j )
	{
		GLushort start1 = (segments - 1) * segments;
		GLushort wrap = (j + 1) % segments;

		inds1 = (idata + (segments - 1) * segments * 6 + j * 3);
		inds2 = (idata + inext + (segments - 1) * segments * 6 + j * 3);

		inds1[0] = start1 + j;
		inds1[1] = start1 + wrap;
		inds1[2] = segments * segments;

		inds2[0] = vnext + start1 + j;
		inds2[1] = vnext + segments * segments;
		inds2[2] = vnext + start1 + wrap;
	}

	inext *= 2;

	// cylinder
	for( GLushort j = 0; j < segments; ++j )
	{
		inds1 = (idata + inext + j * 6);

		inds1[0] = j;					inds1[3] = vnext + j;
		inds1[1] = vnext + j;			inds1[4] = vnext + (j + 1) % segments;
		inds1[2] = (j + 1) % segments;	inds1[5] = (j + 1) % segments;
	}

	(*mesh)->UnlockIndexBuffer();
	(*mesh)->UnlockVertexBuffer();

	box.Min[0] = length * -0.5f - radius;	box.Max[0] = length * 0.5f + radius;
	box.Min[1] = -radius;					box.Max[1] = radius;
	box.Min[2] = -radius;					box.Max[2] = radius;

	(*mesh)->SetAttributeTable(table, 1);
	(*mesh)->SetBoundingBox(box);

	return true;
}

static GLuint GLCompileShader(GLenum type, const char* file, const char* defines)
{
	std::string	source;
	char		log[1024];
	size_t		pos;
	FILE*		infile = 0;
	GLuint		shader = 0;
	GLint		length;
	GLint		success;
	int			deflength = 0;

	if( !file )
		return 0;

	if( !(infile = fopen(file, "rb")) )
		return 0;

	fseek(infile, 0, SEEK_END);
	length = ftell(infile);
	fseek(infile, 0, SEEK_SET);

	if( defines )
		deflength = strlen(defines);

	source.reserve(length + deflength);
	source.resize(length);

	fread(&source[0], 1, length, infile);

	// add defines
	if( defines )
	{
		pos = source.find("#version");
		pos = source.find('\n', pos) + 1;

		source.insert(pos, defines);
	}

	fclose(infile);

	// process includes (non-recursive)
	pos = source.find("#include");

	while( pos != std::string::npos )
	{
		size_t start = source.find('\"', pos) + 1;
		size_t end = source.find('\"', start);

		std::string incfile(source.substr(start, end - start));
		std::string path;
		std::string incsource;

		GLGetPath(path, file);

		infile = fopen((path + incfile).c_str(), "rb");

		if( infile )
		{
			fseek(infile, 0, SEEK_END);
			length = ftell(infile);
			fseek(infile, 0, SEEK_SET);

			incsource.resize(length);

			fread(&incsource[0], 1, length, infile);
			fclose(infile);

			source.replace(pos, end - pos + 1, incsource);
		}

		pos = source.find("#include", end);
	}

	shader = glCreateShader(type);
	length = source.length();

	const GLcharARB* sourcedata = (const GLcharARB*)source.data();

	glShaderSource(shader, 1, &sourcedata, &length);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if( success != GL_TRUE )
	{
		glGetShaderInfoLog(shader, 1024, &length, log);
		log[length] = 0;

		std::cout << log << "\n";
		glDeleteShader(shader);

		return 0;
	}

	return shader;
}

bool GLCreateEffectFromFile(const char* vsfile, const char* gsfile, const char* psfile, OpenGLEffect** effect, const char* defines)
{
	char			log[1024];
	OpenGLEffect*	neweffect;
	GLuint			vertexshader = 0;
	GLuint			fragmentshader = 0;
	GLuint			geometryshader = 0;
	GLint			success;
	GLint			length;

	if( 0 == (vertexshader = GLCompileShader(GL_VERTEX_SHADER, vsfile, defines)) )
		return false;

	if( 0 == (fragmentshader = GLCompileShader(GL_FRAGMENT_SHADER, psfile, defines)) )
	{
		glDeleteShader(vertexshader);
		return false;
	}

	if( gsfile )
	{
		if( 0 == (geometryshader = GLCompileShader(GL_GEOMETRY_SHADER, gsfile, defines)) )
		{
			glDeleteShader(vertexshader);
			glDeleteShader(fragmentshader);

			return false;
		}
	}

	neweffect = new OpenGLEffect();
	neweffect->program = glCreateProgram();

	glAttachShader(neweffect->program, vertexshader);
	glAttachShader(neweffect->program, fragmentshader);

	if( geometryshader )
		glAttachShader(neweffect->program, geometryshader);

	glLinkProgram(neweffect->program);
	glGetProgramiv(neweffect->program, GL_LINK_STATUS, &success);

	if( success != GL_TRUE )
	{
		glGetProgramInfoLog(neweffect->program, 1024, &length, log);
		log[length] = 0;

		std::cout << log << "\n";

		glDeleteShader(vertexshader);
		glDeleteShader(fragmentshader);
		glDeleteProgram(neweffect->program);

		delete neweffect;
		return false;
	}

	neweffect->BindAttributes();
	neweffect->QueryUniforms();

	glDeleteShader(vertexshader);
	glDeleteShader(fragmentshader);

	(*effect) = neweffect;
	return true;
}

bool GLCreateComputeProgramFromFile(const char* csfile, OpenGLEffect** effect, const char* defines)
{
	char			log[1024];
	OpenGLEffect*	neweffect;
	GLuint			shader = 0;
	GLint			success;
	GLint			length;

	if( 0 == (shader = GLCompileShader(GL_COMPUTE_SHADER, csfile, defines)) )
		return false;

	neweffect = new OpenGLEffect();
	neweffect->program = glCreateProgram();

	glAttachShader(neweffect->program, shader);
	glLinkProgram(neweffect->program);
	glGetProgramiv(neweffect->program, GL_LINK_STATUS, &success);

	if( success != GL_TRUE )
	{
		glGetProgramInfoLog(neweffect->program, 1024, &length, log);
		log[length] = 0;

		std::cout << log << "\n";

		glDeleteProgram(neweffect->program);
		delete neweffect;

		return false;
	}

	neweffect->QueryUniforms();
	glDeleteShader(shader);

	(*effect) = neweffect;
	return true;
}

bool GLCreateTessellationProgramFromFile(
	const char* vsfile,
	const char* tcfile,
	const char* tefile,
	const char* gsfile,
	const char* psfile,
	OpenGLEffect** effect)
{
	char			log[1024];
	OpenGLEffect*	neweffect;
	GLuint			vertexshader = 0;
	GLuint			tesscontrolshader = 0;
	GLuint			tessshader = 0;
	GLuint			tessevalshader = 0;
	GLuint			geometryshader = 0;
	GLuint			fragmentshader = 0;
	GLint			success;
	GLint			length;

	// these are mandatory
	if( 0 == (vertexshader = GLCompileShader(GL_VERTEX_SHADER, vsfile, 0)) )
		return false;

	if( 0 == (fragmentshader = GLCompileShader(GL_FRAGMENT_SHADER, psfile, 0)) )
	{
		glDeleteShader(vertexshader);
		return false;
	}

	if( 0 == (tessevalshader = GLCompileShader(GL_TESS_EVALUATION_SHADER, tefile, 0)) )
	{
		glDeleteShader(vertexshader);
		glDeleteShader(fragmentshader);

		return false;
	}

	// others are optional
	if( tcfile )
	{
		if( 0 == (tesscontrolshader = GLCompileShader(GL_TESS_CONTROL_SHADER, tcfile, 0)) )
		{
			glDeleteShader(vertexshader);
			glDeleteShader(fragmentshader);
			glDeleteShader(tessevalshader);

			return false;
		}
	}

	if( gsfile )
	{
		if( 0 == (geometryshader = GLCompileShader(GL_GEOMETRY_SHADER, gsfile, 0)) )
		{
			glDeleteShader(vertexshader);
			glDeleteShader(fragmentshader);
			glDeleteShader(tessevalshader);

			if( tesscontrolshader )
				glDeleteShader(tesscontrolshader);

			return false;
		}
	}

	neweffect = new OpenGLEffect();
	neweffect->program = glCreateProgram();

	glAttachShader(neweffect->program, vertexshader);
	glAttachShader(neweffect->program, tessevalshader);
	glAttachShader(neweffect->program, fragmentshader);

	if( tesscontrolshader )
		glAttachShader(neweffect->program, tesscontrolshader);

	if( geometryshader )
		glAttachShader(neweffect->program, geometryshader);

	glLinkProgram(neweffect->program);
	glGetProgramiv(neweffect->program, GL_LINK_STATUS, &success);

	if( success != GL_TRUE )
	{
		glGetProgramInfoLog(neweffect->program, 1024, &length, log);
		log[length] = 0;

		std::cout << log << "\n";

		glDeleteProgram(neweffect->program);
		delete neweffect;

		return false;
	}

	neweffect->BindAttributes();
	neweffect->QueryUniforms();

	glDeleteShader(vertexshader);
	glDeleteShader(tessevalshader);
	glDeleteShader(fragmentshader);

	if( tesscontrolshader )
		glDeleteShader(tesscontrolshader);

	if( geometryshader )
		glDeleteShader(geometryshader);

	(*effect) = neweffect;
	return true;
}

bool GLCreateTexture(GLsizei width, GLsizei height, GLint miplevels, OpenGLFormat format, GLuint* out)
{
	GLuint texid = 0;

	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if( miplevels != 1 )
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D, 0, map_Format_Internal[format], width, height, 0,
		map_Format_Format[format], map_Format_Type[format], 0);

	if( miplevels != 1 )
		glGenerateMipmap(GL_TEXTURE_2D);

	*out = texid;
	return true;
}

static bool GLCreateTextureFromDDS(const char* file, GLuint* out)
{
	DDS_Image_Info info;
	GLuint texid = 0;

	if( !LoadFromDDS(file, &info) )
	{
		std::cout << "Error: Could not load texture!";
		return false;
	}

	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if( info.MipLevels > 1 )
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLsizei pow2w = GLNextPow2(info.Width);
	GLsizei pow2h = GLNextPow2(info.Height);
	GLsizei mipsize;

	if( info.Format == GLFMT_DXT1 || info.Format == GLFMT_DXT5 )
	{
		// compressed
		GLsizei width = info.Width;
		GLsizei height = info.Height;
		GLsizei offset = 0;

		for( unsigned int j = 0; j < info.MipLevels; ++j )
		{
			mipsize = GetCompressedLevelSize(info.Width, info.Height, j, info.Format);

			glCompressedTexImage2D(GL_TEXTURE_2D, j, map_Format_Internal[info.Format],
				width, height, 0, mipsize, (char*)info.Data + offset);

			offset += mipsize;

			width = (pow2w >> (j + 1));
			height = (pow2h >> (j + 1));
		}
	}
	else
	{
		// uncompressed
		unsigned int bytes = 4;

		if( info.Format == GLFMT_G32R32F )
			bytes = 8;
		else if( info.Format == GLFMT_G16R16F )
			bytes = 4;

		mipsize = info.Width * info.Height * bytes;

		// TODO: itt is mipmap
		glTexImage2D(GL_TEXTURE_2D, 0, map_Format_Internal[info.Format], info.Width, info.Height, 0,
			map_Format_Format[info.Format], map_Format_Type[info.Format], (char*)info.Data);

		if( info.MipLevels > 1 )
			glGenerateMipmap(GL_TEXTURE_2D);
	}

	if( info.Data )
		free(info.Data);

	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
	{
		glDeleteTextures(1, &texid);
		texid = 0;

		std::cout << "Error: Could not create texture!";
	}
	else
		std::cout << "Created texture " << info.Width << "x" << info.Height << "\n";

	*out = texid;
	return (texid != 0);
}

bool GLCreateTextureFromFile(const char* file, bool srgb, GLuint* out)
{
	std::string ext;

	GLGetExtension(ext, file);

	if( ext == "dds" )
		return GLCreateTextureFromDDS(file, out);

	std::wstring wstr;
	int length = strlen(file);
	int size = MultiByteToWideChar(CP_UTF8, 0, file, length, 0, 0);

	wstr.resize(size);
	MultiByteToWideChar(CP_UTF8, 0, file, length, &wstr[0], size);

	GLuint texid = 0;
	Gdiplus::Bitmap* bitmap = Win32LoadPicture(wstr);

	if( bitmap )
	{ 
		if( bitmap->GetLastStatus() == Gdiplus::Ok )
		{
			Gdiplus::BitmapData data;
			unsigned char* tmpbuff;

			bitmap->LockBits(0, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

			tmpbuff = new unsigned char[data.Width * data.Height * 4];
			memcpy(tmpbuff, data.Scan0, data.Width * data.Height * 4);

			for( UINT i = 0; i < data.Height; ++i )
			{
				// swap red and blue
				for( UINT j = 0; j < data.Width; ++j )
				{
					UINT index = (i * data.Width + j) * 4;
					std::swap<unsigned char>(tmpbuff[index + 0], tmpbuff[index + 2]);
				}

				// flip on X
				for( UINT j = 0; j < data.Width / 2; ++j )
				{
					UINT index1 = (i * data.Width + j) * 4;
					UINT index2 = (i * data.Width + (data.Width - j - 1)) * 4;

					std::swap<unsigned int>(*((unsigned int*)(tmpbuff + index1)), *((unsigned int*)(tmpbuff + index2)));
				}
			}

			glGenTextures(1, &texid);
			glBindTexture(GL_TEXTURE_2D, texid);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if( srgb )
				glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, data.Width, data.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmpbuff);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.Width, data.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmpbuff);

			glGenerateMipmap(GL_TEXTURE_2D);

			GLenum err = glGetError();

			if( err != GL_NO_ERROR )
			{
				glDeleteTextures(1, &texid);
				texid = 0;

				std::cout << "Error: Could not create texture!";
			}
			else
				std::cout << "Created texture " << data.Width << "x" << data.Height << "\n";

			bitmap->UnlockBits(&data);
			delete[] tmpbuff;
		}

		delete bitmap;
	}
	else
		std::cout << "Error: Could not load bitmap!";

	*out = texid;
	return (texid != 0);
}

bool GLCreateCubeTextureFromFile(const char* file, GLuint* out)
{
	DDS_Image_Info info;
	GLuint texid = 0;

	if( !LoadFromDDS(file, &info) )
	{
		std::cout << "Error: Could not load texture!";
		return false;
	}

	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texid);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if( info.MipLevels > 1 )
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLsizei pow2s = GLNextPow2(info.Width);
	GLsizei facesize;

	if( info.Format == GLFMT_DXT1 || info.Format == GLFMT_DXT5 )
	{
		// compressed
		GLsizei size;
		GLsizei offset = 0;

		for( int i = 0; i < 6; ++i )
		{
			for( unsigned int j = 0; j < info.MipLevels; ++j )
			{
				size = GLMax(1, pow2s >> j);
				facesize = GetCompressedLevelSize(info.Width, info.Height, j, info.Format);

				glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, map_Format_Internal[info.Format],
					size, size, 0, facesize, (char*)info.Data + offset);

				offset += facesize;
			}
		}
	}
	else
	{
		// uncompressed
		GLsizei size;
		GLsizei offset = 0;
		GLsizei bytes = 4;

		if( info.Format == GLFMT_A16B16G16R16F )
			bytes = 8;

		for( int i = 0; i < 6; ++i )
		{
			for( unsigned int j = 0; j < info.MipLevels; ++j )
			{
				size = GLMax(1, pow2s >> j);
				facesize = size * size * bytes;

				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, map_Format_Internal[info.Format], size, size, 0,
					map_Format_Format[info.Format], map_Format_Type[info.Format], (char*)info.Data + offset);

				offset += facesize;
			}
		}
	}

	if( info.Data )
		free(info.Data);

	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
	{
		glDeleteTextures(1, &texid);
		texid = 0;

		std::cout << "Error: Could not create texture!";
	}
	else
		std::cout << "Created cube texture " << info.Width << "x" << info.Height << "\n";

	*out = texid;
	return (texid != 0);
}

bool GLCreateCubeTextureFromFiles(const char* files[6], bool srgb, GLuint* out)
{
	std::wstring wstr;
	int length, size;
	GLuint texid = 0;

	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texid);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for( int k = 0; k < 6; ++k )
	{
		length = strlen(files[k]);
		size = MultiByteToWideChar(CP_UTF8, 0, files[k], length, 0, 0);

		wstr.resize(size);
		MultiByteToWideChar(CP_UTF8, 0, files[k], length, &wstr[0], size);

		Gdiplus::Bitmap* bitmap = Win32LoadPicture(wstr);

		if( bitmap )
		{ 
			if( bitmap->GetLastStatus() == Gdiplus::Ok )
			{
				Gdiplus::BitmapData data;
				unsigned char* tmpbuff;

				bitmap->LockBits(0, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

				tmpbuff = new unsigned char[data.Width * data.Height * 4];
				memcpy(tmpbuff, data.Scan0, data.Width * data.Height * 4);

				for( UINT i = 0; i < data.Height; ++i )
				{
					// swap red and blue
					for( UINT j = 0; j < data.Width; ++j )
					{
						UINT index = (i * data.Width + j) * 4;
						std::swap<unsigned char>(tmpbuff[index + 0], tmpbuff[index + 2]);
					}

					// flip on X
					for( UINT j = 0; j < data.Width / 2; ++j )
					{
						UINT index1 = (i * data.Width + j) * 4;
						UINT index2 = (i * data.Width + (data.Width - j - 1)) * 4;

						std::swap<unsigned int>(*((unsigned int*)(tmpbuff + index1)), *((unsigned int*)(tmpbuff + index2)));
					}
				}

				if( srgb )
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + k, 0, GL_SRGB8_ALPHA8, data.Width, data.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmpbuff);
				else
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + k, 0, GL_RGBA, data.Width, data.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmpbuff);
			
				bitmap->UnlockBits(&data);
				delete[] tmpbuff;
			}

			delete bitmap;
		}
		else
			std::cout << "Error: Could not load bitmap!";
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	GLenum err = glGetError();

	if( err != GL_NO_ERROR )
	{
		glDeleteTextures(1, &texid);
		texid = 0;

		std::cout << "Error: Could not create cube texture!";
	}
	else
		std::cout << "Created cube texture\n";

	*out = texid;
	return (texid != 0);
}

bool GLSaveFP16CubemapToFile(const char* filename, GLuint texture)
{
	DDS_Image_Info	info;
	GLuint			levelsize;
	GLint			size;
	bool			success;

	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
	glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &size);

	levelsize = GetImageSize(size, size, 8, 1);

	info.Width		= size;
	info.Height		= size;
	info.Format		= GLFMT_A16B16G16R16F;
	info.MipLevels	= 1;

	info.DataSize = levelsize * 6;
	info.Data = malloc(info.DataSize);

	for( int i = 0; i < 6; ++i )
	{
		glGetTexImage(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, map_Format_Format[GLFMT_A16B16G16R16F],
			map_Format_Type[GLFMT_A16B16G16R16F], (char*)info.Data + i * levelsize);
	}

	success = SaveToDDS(filename, &info);
	free(info.Data);

	return success;
}

void GLKillAnyRogueObject()
{
	if( gdiplustoken )
		Gdiplus::GdiplusShutdown(gdiplustoken);
}

void GLRenderText(const std::string& str, uint32_t tex, GLsizei width, GLsizei height)
{
	GLRenderTextEx(str, tex, width, height, L"Arial", 1, Gdiplus::FontStyleBold, 25);
}

void GLRenderTextEx(const std::string& str, uint32_t tex, GLsizei width, GLsizei height, const WCHAR* face, bool border, int style, float emsize)
{
	if( tex == 0 )
		return;

	if( gdiplustoken == 0 )
	{
		Gdiplus::GdiplusStartupInput gdiplustartup;
		Gdiplus::GdiplusStartup(&gdiplustoken, &gdiplustartup, NULL);
	}

	Gdiplus::Color				outline(0xff000000);
	Gdiplus::Color				fill(0xffffffff);

	Gdiplus::Bitmap*			bitmap;
	Gdiplus::Graphics*			graphics;
	Gdiplus::GraphicsPath		path;
	Gdiplus::FontFamily			family(face);
	Gdiplus::StringFormat		format;
	Gdiplus::Pen				pen(outline, 3);
	Gdiplus::SolidBrush			brush(border ? fill : outline);
	std::wstring				wstr(str.begin(), str.end());

	bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
	graphics = new Gdiplus::Graphics(bitmap);

	// render text
	graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics->SetPageUnit(Gdiplus::UnitPixel);

	path.AddString(wstr.c_str(), wstr.length(), &family, style, emsize, Gdiplus::Point(0, 0), &format);
	pen.SetLineJoin(Gdiplus::LineJoinRound);

	if( border )
		graphics->DrawPath(&pen, &path);

	graphics->FillPath(&brush, &path);

	// copy to texture
	Gdiplus::Rect rc(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
	Gdiplus::BitmapData data;

	memset(&data, 0, sizeof(Gdiplus::BitmapData));
	bitmap->LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data.Scan0);

	bitmap->UnlockBits(&data);

	delete graphics;
	delete bitmap;
}

void GLSetTexture(GLenum unit, GLenum target, uint32_t texture)
{
	glActiveTexture(unit);
	glBindTexture(target, texture);
}
