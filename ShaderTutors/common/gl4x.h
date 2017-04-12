
#ifndef _GLEXT_H_
#define _GLEXT_H_

#include <cassert>
#include <map>

#include "../extern/qglextensions.h"
#include "orderedarray.hpp"
#include "3Dmath.h"

#ifdef _DEBUG
#	define GL_ASSERT(x)	assert(x)
#else
#	define GL_ASSERT(x)	if( !(x) ) throw 1
#endif

#define GL_SAFE_DELETE_TEXTURE(x) \
	if( x ) { \
		glDeleteTextures(1, &x); \
		x = 0; }
// END

// NOTE: you can freak out.

enum OpenGLDeclType
{
	GLDECLTYPE_FLOAT1 =  0,
	GLDECLTYPE_FLOAT2 =  1,
	GLDECLTYPE_FLOAT3 =  2,
	GLDECLTYPE_FLOAT4 =  3,
	GLDECLTYPE_GLCOLOR =  4
};

enum OpenGLDeclUsage
{
	GLDECLUSAGE_POSITION = 0,
	GLDECLUSAGE_BLENDWEIGHT,
	GLDECLUSAGE_BLENDINDICES,
	GLDECLUSAGE_NORMAL,
	GLDECLUSAGE_PSIZE,
	GLDECLUSAGE_TEXCOORD,
	GLDECLUSAGE_TANGENT,
	GLDECLUSAGE_BINORMAL,
	GLDECLUSAGE_TESSFACTOR,
	GLDECLUSAGE_POSITIONT,
	GLDECLUSAGE_COLOR,
	GLDECLUSAGE_FOG,
	GLDECLUSAGE_DEPTH,
	GLDECLUSAGE_SAMPLE
};

enum OpenGLFormat
{
	GLFMT_UNKNOWN = 0,
	GLFMT_R8,
	GLFMT_R8G8,
	GLFMT_R8G8B8,
	GLFMT_A8R8G8B8,
	GLFMT_sA8R8G8B8,
	GLFMT_D24S8,
	GLFMT_D32F,

	GLFMT_DXT1,
	GLFMT_DXT5,

	GLFMT_R16F,
	GLFMT_G16R16F,
	GLFMT_A16B16G16R16F,

	GLFMT_R32F,
	GLFMT_G32R32F,
	GLFMT_A32B32G32R32F
};

enum OpenGLPrimitiveType
{
	GLPT_POINTLIST = GL_POINTS,
	GLPT_LINELIST = GL_LINES,
	GLPT_TRIANGLELIST = GL_TRIANGLES
};

enum OpenGLLockFlags
{
	GLLOCK_READONLY = GL_MAP_READ_BIT,
	GLLOCK_DISCARD = GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_WRITE_BIT
};

enum OpenGLMeshFlags
{
	GLMESH_DYNAMIC = 1,
	GLMESH_32BIT = 2
};

enum OpenGLTextureFlags
{
	GLTEX_FLIPX = 1
};

struct OpenGLCommonVertex
{
	float x, y, z;
	float nx, ny, nz;
	float u, v;
};

struct OpenGLTBNVertex
{
	float x, y, z;		// 0
	float nx, ny, nz;	// 12
	float u, v;			// 24
	float tx, ty, tz;	// 32
	float bx, by, bz;	// 44
};

struct OpenGLVertexElement
{
	GLushort	Stream;
	GLushort	Offset;
	GLubyte		Type;			// OpenGLDeclType
	GLubyte		Usage;			// OpenGLDeclUsage
	GLubyte		UsageIndex;
};

struct OpenGLVertexDeclaration
{
	OpenGLVertexElement*	Elements;
	GLuint					Stride;
};

struct OpenGLAttributeRange
{
	GLenum		PrimitiveType;	// OpenGLPrimitiveType
	GLuint		AttribId;
	GLuint		IndexStart;
	GLuint		IndexCount;
	GLuint		VertexStart;
	GLuint		VertexCount;
	GLboolean	Enabled;
};

struct OpenGLMaterial
{
	OpenGLColor	Diffuse;
	OpenGLColor	Ambient;
	OpenGLColor	Specular;
	OpenGLColor	Emissive;
	float		Power;
	GLuint		Texture;
	GLuint		NormalMap;

	OpenGLMaterial();
	~OpenGLMaterial();
};

/**
 * \brief Don't load content items more than once.
 */
class OpenGLContentRegistry
{
	typedef std::map<std::string, GLuint> TextureMap;

private:
	static OpenGLContentRegistry* _inst;

	TextureMap textures;

	OpenGLContentRegistry();
	~OpenGLContentRegistry();

public:
	static OpenGLContentRegistry& Instance();
	static void Release();

	void RegisterTexture(const std::string& file, GLuint tex);
	void UnregisterTexture(GLuint tex);

	GLuint IDTexture(const std::string& file);
};

inline OpenGLContentRegistry& OpenGLContentManager() {
	return OpenGLContentRegistry::Instance();
}

/**
 * \brief Similar to ID3DXMesh. One stream, core profile only.
 */
class OpenGLMesh
{
	friend bool GLCreateMesh(GLuint, GLuint, GLuint, OpenGLVertexElement*, OpenGLMesh**);
	friend bool GLCreateMeshFromQM(const char*, OpenGLMesh**);

	struct locked_data
	{
		void* ptr;
		GLuint flags;
	};

private:
	OpenGLAABox					boundingbox;
	OpenGLAttributeRange*		subsettable;
	OpenGLMaterial*				materials;
	OpenGLVertexDeclaration		vertexdecl;
	
	GLuint						meshoptions;
	GLuint						numsubsets;
	GLuint						numvertices;
	GLuint						numindices;
	GLuint						vertexbuffer;
	GLuint						indexbuffer;
	GLuint						vertexlayout;

	locked_data					vertexdata_locked;
	locked_data					indexdata_locked;

	OpenGLMesh();

	void Destroy();
	void RecreateVertexLayout();

public:
	~OpenGLMesh();

	bool LockVertexBuffer(GLuint offset, GLuint size, GLuint flags, void** data);
	bool LockIndexBuffer(GLuint offset, GLuint size, GLuint flags, void** data);

	void DrawSubset(GLuint subset, bool bindtextures = false);
	void EnableSubset(GLuint subset, bool enable);
	void GenerateTangentFrame();
	void ReorderSubsets(GLuint newindices[]);
	void UnlockVertexBuffer();
	void UnlockIndexBuffer();
	void SetAttributeTable(const OpenGLAttributeRange* table, GLuint size);

	inline void SetBoundingBox(const OpenGLAABox& box) {
		boundingbox = box;
	}

	inline OpenGLAttributeRange* GetAttributeTable() {
		return subsettable;
	}

	inline OpenGLMaterial* GetMaterialTable() {
		return materials;
	}

	inline const OpenGLAABox& GetBoundingBox() const {
		return boundingbox;
	}

	inline size_t GetNumBytesPerVertex() const {
		return vertexdecl.Stride;
	}

	inline GLuint GetNumSubsets() const {
		return numsubsets;
	}

	inline GLuint GetVertexLayout() const {
		return vertexlayout;
	}

	inline GLuint GetVertexBuffer() const {
		return vertexbuffer;
	}

	inline GLuint GetIndexBuffer() const {
		return indexbuffer;
	}
};

/**
 * \brief Similar to ID3DXEffect. One technique, core profile only.
 */
class OpenGLEffect
{
	// this is a bad habit...
	friend bool GLCreateEffectFromFile(const char*, const char*, const char*, OpenGLEffect**, const char*);
	friend bool GLCreateComputeProgramFromFile(const char*, OpenGLEffect**, const char*);
	friend bool GLCreateTessellationProgramFromFile(const char*, const char*, const char*, const char*, const char*, OpenGLEffect**);

	struct Uniform
	{
		char	Name[32];
		GLint	StartRegister;
		GLint	RegisterCount;
		GLint	Location;
		GLenum	Type;

		mutable bool Changed;

		inline bool operator <(const Uniform& other) const {
			return (0 > strcmp(Name, other.Name));
		}
	};

	typedef mystl::orderedarray<Uniform> uniformtable;

private:
	uniformtable	uniforms;
	GLuint			program;
	float*			floatvalues;
	int*			intvalues;
	unsigned int	floatcap;
	unsigned int	floatsize;
	unsigned int	intcap;
	unsigned int	intsize;

	OpenGLEffect();

	void AddUniform(const char* name, GLuint location, GLuint count, GLenum type);
	void BindAttributes();
	void Destroy();
	void QueryUniforms();

public:
	~OpenGLEffect();

	void Begin();
	void CommitChanges();
	void End();
	void SetMatrix(const char* name, const float* value);
	void SetVector(const char* name, const float* value);
	void SetVectorArray(const char* name, const float* values, GLsizei count);
	void SetFloat(const char* name, float value);
	void SetFloatArray(const char* name, const float* values, GLsizei count);
	void SetInt(const char* name, int value);
};

/**
 * \brief FBO with attachments
 */
class OpenGLFramebuffer
{
	struct Attachment
	{
		GLuint id;
		int type;

		Attachment()
			: id(0), type(0) {}
	};

private:
	GLuint		fboid;
	GLuint		sizex;
	GLuint		sizey;
	Attachment	rendertargets[8];
	Attachment	depthstencil;

public:
	OpenGLFramebuffer(GLuint width, GLuint height);
	~OpenGLFramebuffer();

	bool AttachRenderbuffer(GLenum target, OpenGLFormat format, GLsizei samples = 1);
	bool AttachCubeTexture(GLenum target, OpenGLFormat format, GLenum filter = GL_NEAREST);
	bool AttachTexture(GLenum target, OpenGLFormat format, GLenum filter = GL_NEAREST);
	bool Validate();

	void Attach(GLenum target, GLuint tex, GLint level);
	void Detach(GLenum target);
	void Reattach(GLenum target, GLint level);
	void Reattach(GLenum target, GLint face, GLint level);
	void Resolve(OpenGLFramebuffer* to, GLbitfield mask);
	void Set();
	void Unset();

	inline GLuint GetFramebuffer() const {
		return fboid;
	}

	inline GLuint GetColorAttachment(int index) const {
		return rendertargets[index].id;
	}
	
	inline GLuint GetDepthAttachment() const {
		return depthstencil.id;
	}

	inline GLuint GetWidth() const {
		return sizex;
	}

	inline GLuint GetHeight() const {
		return sizey;
	}
};

/**
 * \brief Simple quad for 2D rendering
 */
class OpenGLScreenQuad
{
private:
	GLuint vertexbuffer;
	GLuint vertexlayout;

public:
	OpenGLScreenQuad();
	~OpenGLScreenQuad();

	void Draw();
};

// content functions
bool GLCreateTexture(GLsizei width, GLsizei height, GLint miplevels, OpenGLFormat format, GLuint* out, void* data = 0);
bool GLCreateTextureFromFile(const char* file, bool srgb, GLuint* out, GLuint flags = 0);
bool GLCreateCubeTextureFromFile(const char* file, GLuint* out);
bool GLCreateCubeTextureFromFiles(const char* files[6], bool srgb, GLuint* out);
bool GLCreateMesh(GLuint numvertices, GLuint numindices, GLuint options, OpenGLVertexElement* decl, OpenGLMesh** mesh);
bool GLCreateMeshFromQM(const char* file, OpenGLMesh** mesh);
bool GLCreatePlane(float width, float height, float uscale, float vscale, OpenGLMesh** mesh);
bool GLCreateBox(float width, float height, float depth, float uscale, float vscale, float wscale, OpenGLMesh** mesh);
bool GLCreateCapsule(float length, float radius, OpenGLMesh** mesh);
bool GLCreateEffectFromFile(const char* vsfile, const char* gsfile, const char* psfile, OpenGLEffect** effect, const char* defines = 0);
bool GLCreateComputeProgramFromFile(const char* csfile, OpenGLEffect** effect, const char* defines = 0);
bool GLCreateTessellationProgramFromFile(
	const char* vsfile,
	const char* tcfile,
	const char* tefile,
	const char* gsfile,
	const char* psfile,
	OpenGLEffect** effect);

bool GLSaveFP16CubemapToFile(const char* filename, GLuint texture);

// other
void GLKillAnyRogueObject();
void GLRenderText(const std::string& str, GLuint tex, GLsizei width, GLsizei height);
void GLRenderTextEx(
	const std::string& str,
	GLuint tex,
	GLsizei width,
	GLsizei height,
	const WCHAR* face,
	bool border,
	int style,
	float emsize);

void GLSetTexture(GLenum unit, GLenum target, GLuint texture);

#endif
