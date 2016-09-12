//
//  Renderer.m
//  70_RandomAccess
//
//  Created by iszennai on 21/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#import "MetalView.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

#include "../common/3Dmath.h"

#define NUM_QUEUED_FRAMES	3

@class MetalMesh;

id<MTLDevice>				mtldevice;
id<MTLCommandQueue>			commandqueue;
id<MTLLibrary>				defaultlibrary;
id<MTLComputePipelineState>	computestate;
id<MTLRenderPipelineState>  renderstate;
id<MTLDepthStencilState>	depthstate;
id<MTLBuffer>				computeuniforms[NUM_QUEUED_FRAMES];
id<MTLBuffer>				renderuniforms[NUM_QUEUED_FRAMES];
id<MTLBuffer>				materials;
id<MTLTexture>				texture;
id<MTLTexture>				depthtex;

id<MTLBuffer>				batch_physcical_vbo;
id<MTLBuffer>				batch_physcical_ibo;

MTLRenderPassDescriptor*	renderpassdesc;
MetalMesh*					mesh;
AVAudioPlayer*				player;
dispatch_semaphore_t		framesema;
NSUInteger					label1;

int							currentframe = 0;
int							framerate = 0;
int							frametime = 0;

unsigned int				num_logical_batches;
unsigned int				teapots_per_logical_batch;
unsigned int				logical_batch_vbo_size; // must be multiple of 4
unsigned int				logical_batch_ibo_size;
unsigned int				teapotvertices;
unsigned int				teapotindices;
unsigned int				grid_width;
unsigned int				grid_depth;

struct CommonVertex
{
	float pos[3];
	float norm[3];
	float tex[2];
};

struct CompressedVertex
{
	float pos[3];
	unsigned int norm;
	unsigned short tex[2];
	unsigned short matID;
}; // 12 + 4 + 4 + 2 = 22 B

void UpdateLabels(MetalView* view);

// *****************************************************************************************************************************
//
// MetalMesh class
//
// *****************************************************************************************************************************

@interface MetalMesh : NSObject

@property(nonatomic, readonly, strong) id<MTLBuffer> vertexBuffer;
@property(nonatomic, readonly, strong) id<MTLBuffer> indexBuffer;
@property(readonly) NSUInteger numVertices;
@property(readonly) NSUInteger numIndices;

@property(nonatomic, readonly) CompressedVertex* vertexData;
@property(nonatomic, readonly) unsigned short* indexData;

+ (MetalMesh*)loadFromQM:(NSString*)file createBuffers:(BOOL)createbuffers;

- (id)initWithBuffers:(id<MTLBuffer>)vertexbuffer indexBuffer:(id<MTLBuffer>)indexbuffer vertexCount:(NSUInteger)vertexcount indexCount:(NSUInteger)indexcount;
- (id)initWithData:(CompressedVertex*)vdata indexData:(unsigned short*)idata vertexCount:(NSUInteger)vertexcount indexCount:(NSUInteger)indexcount;

@end

@implementation MetalMesh

@synthesize vertexBuffer = _vertexBuffer;
@synthesize indexBuffer = _indexBuffer;
@synthesize numVertices = _numVertices;
@synthesize numIndices = _numIndices;
@synthesize vertexData = _vertexData;
@synthesize indexData = _indexData;

+ (MetalMesh*)loadFromQM:(NSString*)file createBuffers:(BOOL)createbuffers
{
	static const unsigned short elemsizes[6] =
	{
		1, 2, 3, 4, 4, 4
	};
	
	static const unsigned short elemstrides[6] =
	{
		4, 4, 4, 4, 1, 1
	};
	
	FILE* infile = fopen([file UTF8String], "rb");
	
	if (!infile)
		return 0;
	
	unsigned int unused;
	unsigned int numvertices;
	unsigned int numindices;
	unsigned int istride;
	unsigned int vstride = 0;
	unsigned int numsubsets;
	unsigned int version;
	unsigned char elemtype;
	
	fread(&unused, 4, 1, infile);
	fread(&numindices, 4, 1, infile);
	fread(&istride, 4, 1, infile);
	fread(&numsubsets, 4, 1, infile);
	
	version = unused >> 16;
	
	fread(&numvertices, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	
	// vertex decl
	fread(&unused, 4, 1, infile);
	
	for (unsigned int i = 0; i < unused; ++i) {
		fseek(infile, 3, SEEK_CUR);
		fread(&elemtype, 1, 1, infile);
		fseek(infile, 1, SEEK_CUR);
		
		vstride += elemsizes[elemtype] * elemstrides[elemtype];
	}
	
	void* vdata = malloc(numvertices * vstride);
	fread(vdata, vstride, numvertices, infile);
	
	CompressedVertex* compdata = (CompressedVertex*)malloc(numvertices * sizeof(CompressedVertex));
	
	for (unsigned int i = 0; i < numvertices; ++i) {
		CompressedVertex& compvert = compdata[i];
		CommonVertex& vert = ((CommonVertex*)vdata)[i];
		
		compvert.pos[0] = vert.pos[0];
		compvert.pos[1] = vert.pos[1];
		compvert.pos[2] = vert.pos[2];
		
		compvert.tex[0] = MTLFloatToHalf(vert.tex[0]);
		compvert.tex[1] = MTLFloatToHalf(vert.tex[1]);
		
		compvert.norm = MTLVec3ToUbyte4(vert.norm);
		compvert.matID = 0;
	}
	
	id<MTLBuffer> vbo = nil;
	id<MTLBuffer> ibo = nil;
	
	if (createbuffers) {
		vbo = [mtldevice newBufferWithBytes:compdata length:numvertices * sizeof(CompressedVertex) options:MTLResourceOptionCPUCacheModeDefault];
		free(compdata);
	}
	
	free(vdata);
	
	void* idata = malloc(numindices * istride);
	fread(idata, istride, numindices, infile);
	
	if (createbuffers) {
		ibo = [mtldevice newBufferWithBytes:idata length:numindices * istride options:MTLResourceOptionCPUCacheModeDefault];
		free(idata);
	}
	
	// skip subset and material info
	fclose(infile);
	
	if (createbuffers)
		return [[MetalMesh alloc] initWithBuffers:vbo indexBuffer:ibo vertexCount:numvertices indexCount:numindices];
	
	return [[MetalMesh alloc] initWithData:compdata indexData:(unsigned short*)idata vertexCount:numvertices indexCount:numindices];
}

- (id)initWithBuffers:(id<MTLBuffer>)vertexbuffer indexBuffer:(id<MTLBuffer>)indexbuffer vertexCount:(NSUInteger)vertexcount indexCount:(NSUInteger)indexcount
{
	self = [super init];
	
	if (self) {
		_vertexBuffer = vertexbuffer;
		_indexBuffer = indexbuffer;
		_numVertices = vertexcount;
		_numIndices = indexcount;
		_vertexData = nil;
		_indexData = nil;
	}
	
	return self;
}

- (id)initWithData:(CompressedVertex*)vdata indexData:(unsigned short*)idata vertexCount:(NSUInteger)vertexcount indexCount:(NSUInteger)indexcount
{
	self = [super init];
	
	if (self) {
		_vertexBuffer = nil;
		_indexBuffer = nil;
		_numVertices = vertexcount;
		_numIndices = indexcount;
		_vertexData = vdata;
		_indexData = idata;
	}
	
	return self;
}

- (void)dealloc
{
	_vertexBuffer = nil;
	_indexBuffer = nil;
	
	if (_vertexData)
		free(_vertexData);
	
	if (_indexData)
		free(_indexData);
}

@end

// *****************************************************************************************************************************
//
// Sample impl
//
// *****************************************************************************************************************************

BOOL InitScene(MetalView* view)
{
	srand((unsigned int)time(0));
	
	framesema = dispatch_semaphore_create(NUM_QUEUED_FRAMES);
	
	mtldevice = [view device];
	commandqueue = [mtldevice newCommandQueue];
	defaultlibrary = [mtldevice newDefaultLibrary];

	mesh = [MetalMesh loadFromQM:[[NSBundle mainBundle] pathForResource:@"teapot" ofType:@"qm"] createBuffers:NO];
	
	if (!mesh) {
		NSLog(@"Error: Could not load mesh!");
		return NO;
	}
	
	// teapot size is: 3.217, 1.575, 2.0
	
	teapotvertices							= (unsigned int)[mesh numVertices];
	teapotindices							= (unsigned int)[mesh numIndices];
	teapots_per_logical_batch				= 65536 / teapotvertices; // 7 teapots (60522 vertices)
	
	unsigned int physical_batch_size		= 256 * 1024 * 1024;
	unsigned int inds_per_verts				= teapotindices / teapotvertices; // 5
	
	logical_batch_vbo_size					= teapotindices / inds_per_verts * sizeof(CompressedVertex) * teapots_per_logical_batch;	// 1651440
	logical_batch_ibo_size					= teapotindices * sizeof(unsigned short) * teapots_per_logical_batch;						// 688128
	num_logical_batches						= physical_batch_size / (logical_batch_vbo_size + logical_batch_ibo_size);
	
	unsigned int physical_batch_vbo_size	= num_logical_batches * logical_batch_vbo_size;
	unsigned int physical_batch_ibo_size	= num_logical_batches * logical_batch_ibo_size;
	unsigned int vbo_difference				= logical_batch_vbo_size - teapots_per_logical_batch * teapotvertices * sizeof(CompressedVertex);
	unsigned int ibo_difference				= logical_batch_ibo_size - teapots_per_logical_batch * teapotindices * sizeof(unsigned short);
	
	batch_physcical_vbo = [mtldevice newBufferWithLength:physical_batch_vbo_size options:MTLResourceOptionCPUCacheModeDefault|MTLResourceStorageModeShared];
	batch_physcical_ibo = [mtldevice newBufferWithLength:physical_batch_ibo_size options:MTLResourceOptionCPUCacheModeDefault|MTLResourceStorageModeShared];

	// create static batches
	CompressedVertex*	batch_vdata			= (CompressedVertex*)[batch_physcical_vbo contents];
	CompressedVertex*	vdata				= [mesh vertexData];
	unsigned short*		batch_idata			= (unsigned short*)[batch_physcical_ibo contents];
	unsigned short*		idata				= [mesh indexData];
	unsigned short		materialid;
	unsigned int		maxcells			= num_logical_batches * teapots_per_logical_batch;
	unsigned int		cellcount			= MTLISqrt(maxcells) + 1;
	unsigned int		teapots				= 0;
	
	grid_width = (unsigned int)(cellcount * 3.5f) + 1;
	grid_depth = (unsigned int)(cellcount * 2.3f) + 1;
	
	float				cell_width			= grid_width / (float)cellcount;
	float				cell_depth			= grid_depth / (float)cellcount;
	float				xoffset;
	float				zoffset;
	
	for (unsigned int i = 0; i < cellcount; ++i) {
		for (unsigned int j = 0; j < cellcount; ++j) {
			
			xoffset = j * cell_width + cell_width * 0.5f;
			zoffset = i * cell_depth + cell_depth * 0.5f;
			
			materialid = rand() % 16;
			
			for (unsigned int k = 0; k < teapotvertices; ++k) {
				batch_vdata[k].pos[0] = vdata[k].pos[0] + xoffset;
				batch_vdata[k].pos[1] = vdata[k].pos[1];
				batch_vdata[k].pos[2] = vdata[k].pos[2] + zoffset;
				
				batch_vdata[k].tex[0] = vdata[k].tex[0];
				batch_vdata[k].tex[1] = vdata[k].tex[1];
				
				batch_vdata[k].norm = vdata[k].norm;
				batch_vdata[k].matID = materialid;
			}
			
			for (unsigned int k = 0; k < teapotindices; ++k) {
				batch_idata[k] = idata[k] + (teapots % teapots_per_logical_batch) * teapotvertices;
			}
			
			batch_vdata += teapotvertices;
			batch_idata += teapotindices;
			
			++teapots;
			
			if (teapots >= maxcells)
				break;
			
			if (teapots % teapots_per_logical_batch == 0) {
				// skip difference
				unsigned char* vbytes = (unsigned char*)batch_vdata;
				unsigned char* ibytes = (unsigned char*)batch_idata;
				
				vbytes += vbo_difference;
				batch_vdata = (CompressedVertex*)vbytes;
				
				ibytes += ibo_difference;
				batch_idata = (unsigned short*)ibytes;
			}
		}
		
		if (teapots >= maxcells)
			break;
	}
	
	mesh = nil;
	
	// create render pipeline state
	MTLRenderPipelineDescriptor* pipelinedesc = [[MTLRenderPipelineDescriptor alloc] init];
	
	pipelinedesc.vertexFunction = [defaultlibrary newFunctionWithName:@"vs_main"];
	pipelinedesc.fragmentFunction = [defaultlibrary newFunctionWithName:@"ps_main"];
	pipelinedesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
	pipelinedesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
	
	MTLDepthStencilDescriptor* depthdesc = [[MTLDepthStencilDescriptor alloc] init];
	
	depthdesc.depthCompareFunction = MTLCompareFunctionLess;
	depthdesc.depthWriteEnabled = YES;
	
	depthstate = [mtldevice newDepthStencilStateWithDescriptor:depthdesc];

	NSError* error = nil;
	renderstate = [mtldevice newRenderPipelineStateWithDescriptor:pipelinedesc error:&error];
	
	if (!renderstate) {
		NSLog(@"Error: %@", error);
		return NO;
	}
	
	computestate = [mtldevice newComputePipelineStateWithFunction:[defaultlibrary newFunctionWithName:@"coloredgrid"] error:&error];
	
	if (!computestate) {
		NSLog(@"Error: %@", error);
		return NO;
	}
	
	for (int i = 0; i < NUM_QUEUED_FRAMES; ++i) {
		computeuniforms[i] = [mtldevice newBufferWithLength:sizeof(float) options:0];
		renderuniforms[i] = [mtldevice newBufferWithLength:52 * sizeof(float) options:0];
	}
	
	materials = [mtldevice newBufferWithLength:16 * 4 * sizeof(float) options:0];
	float (*matdata)[4] = (float (*)[4])[materials contents];
	
	MTLVec4Set(matdata[0], 1, 0, 0, 1);
	MTLVec4Set(matdata[1], 0, 1, 0, 1);
	MTLVec4Set(matdata[2], 0, 0, 1, 1);
	MTLVec4Set(matdata[3], 1, 0, 1, 1);
	MTLVec4Set(matdata[4], 1, 1, 0, 1);
	MTLVec4Set(matdata[5], 0, 1, 1, 1);
	MTLVec4Set(matdata[6], 0.0473f, 0.2176f, 1, 1);
	MTLVec4Set(matdata[7], 0.2176f, 0.0473f, 1, 1);
	MTLVec4Set(matdata[8], 1, 0.0473f, 0.2176f, 1);
	MTLVec4Set(matdata[9], 1, 0.2176f, 0.0473f, 1);
	MTLVec4Set(matdata[10], 0.0473f, 1, 0.2176f, 1);
	MTLVec4Set(matdata[11], 0.2176f, 1, 0.0473f, 1);
	MTLVec4Set(matdata[12], 0, 1, 0.2176f, 1);
	MTLVec4Set(matdata[13], 1, 0, 0.2176f, 1);
	MTLVec4Set(matdata[14], 0.2176f, 0, 1, 1);
	MTLVec4Set(matdata[15], 0, 0.2176f, 1, 1);
	
	// texture
	MTLTextureDescriptor* texdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB width:128 height:128 mipmapped:NO];
	texture = [mtldevice newTextureWithDescriptor:texdesc];
	
	NSURL* musicurl = [[NSURL alloc] initFileURLWithPath:[[NSBundle mainBundle] pathForResource:@"breakingthelaw" ofType:@"mp3"]];
	player = [[AVAudioPlayer alloc] initWithContentsOfURL:musicurl fileTypeHint:AVFileTypeMPEGLayer3 error:nil];
	
	player.numberOfLoops = 0; // play once
	
	label1 = [view addLabel];
	UpdateLabels(view);
	
	return YES;
}

void UpdateLabels(MetalView* view)
{
	[view setLabelText:label1 text:[NSString stringWithFormat:@"Framerate: %d fps (%d ms)", framerate, frametime]];
}

void UninitScene()
{
	[player stop];
	player = nil;
	
	for (int i = 0; i < NUM_QUEUED_FRAMES; ++i) {
		computeuniforms[i] = nil;
		renderuniforms[i] = nil;
	}
	
	batch_physcical_vbo = nil;
	batch_physcical_ibo = nil;
	materials = nil;
	
	mesh = nil;
	texture = nil;
	computestate = nil;
	renderstate = nil;
	depthstate = nil;
	
	defaultlibrary = nil;
	commandqueue = nil;
	mtldevice = nil;
}

void Update(float delta)
{
}

void Render(MetalView* view, float alpha, float elapsedtime)
{
	static float time = 0;
	static float lasttime = 0;
	static float lastmusictime = 0;
	static BOOL wasplaying = NO;
	
	BOOL isplaying = [player isPlaying];
	
	if (wasplaying && !isplaying) {
		lastmusictime = time;
	} else if (time - lastmusictime > 2.0f && !isplaying) {
		[player play];
		isplaying = YES;
	}
	
	wasplaying = isplaying;
	
#if NUM_QUEUED_FRAMES > 1
	// decrement semaphore
	dispatch_semaphore_wait(framesema, DISPATCH_TIME_FOREVER);
#endif
	
	id<CAMetalDrawable>				drawable		= [view currentDrawable];
	id<MTLCommandBuffer>			commandbuffer	= [commandqueue commandBuffer];
	id<MTLRenderCommandEncoder>		renderencoder;
	id<MTLComputeCommandEncoder>	computeencoder;
	
	if (!drawable)
	{
		renderpassdesc = nil;
		return;
	}
	
	if (!renderpassdesc)
		renderpassdesc = [MTLRenderPassDescriptor renderPassDescriptor];
	
	// add the texture some color
	computeencoder = [commandbuffer computeCommandEncoder];
	
	*((float*)[computeuniforms[currentframe] contents]) = time;
	
	[computeencoder setTexture:texture atIndex:0];
	[computeencoder setBuffer:computeuniforms[currentframe] offset:0 atIndex:0];
	[computeencoder setComputePipelineState:computestate];
	[computeencoder dispatchThreadgroups:MTLSizeMake(8, 8, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
	[computeencoder endEncoding];
	
	// setup framebuffer
	MTLRenderPassColorAttachmentDescriptor* colorattachment0 = renderpassdesc.colorAttachments[0];

	colorattachment0.texture = drawable.texture;
	colorattachment0.loadAction = MTLLoadActionClear;
	colorattachment0.storeAction = MTLStoreActionStore;
	colorattachment0.clearColor = MTLClearColorMake(0.0f, 0.0103f, 0.0707f, 1.0f);
	
	bool needsupdate = true;
	
	if (depthtex)
		needsupdate = (depthtex.width != drawable.texture.width || depthtex.height != drawable.texture.height);
	
	if (needsupdate) {
		MTLTextureDescriptor* depthdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
																							 width:drawable.texture.width
																							height:drawable.texture.height
																						 mipmapped:NO];
		
		depthdesc.usage = MTLTextureUsageUnknown;
		depthdesc.storageMode = MTLStorageModePrivate;
		
		depthtex = [mtldevice newTextureWithDescriptor:depthdesc];
		
		MTLRenderPassDepthAttachmentDescriptor* depthattachment = renderpassdesc.depthAttachment;
		
		depthattachment.texture = depthtex;
		depthattachment.loadAction = MTLLoadActionClear;
		depthattachment.storeAction = MTLStoreActionDontCare;
		depthattachment.clearDepth = 1.0;
	}
	
	// setup matrices
	float eye[3];
	float look[3];
	float tang[3];
	float up[3] = { 0, 1, 0 };
	
	float tmp1[16];
	float tmp2[16];
	
	float* udata = (float*)[renderuniforms[currentframe] contents];
	float aspect = [view bounds].size.width / [view bounds].size.height;
	
	// animate eye position
	float cosu = cosf(time * 0.25f);
	float sinu = sinf(time * 0.25f);
	float halfw = grid_width * 0.5f;
	float halfd = grid_depth * 0.5f;
	
	eye[0] = halfw + cosu * halfw * 0.75f;
	eye[1] = 3;
	eye[2] = halfd + sinu * cosu * grid_depth * 0.75f;
	
	tang[0] = -sinu;
	tang[1] = -0.5f;
	tang[2] = cosu * cosu - sinu * sinu;
	
	MTLVec3Normalize(tang, tang);
	MTLVec3Add(look, eye, tang);
	MTLMatrixIdentity(udata); // world
	
	MTLMatrixLookAtLH(tmp1, eye, look, up);
	MTLMatrixPerspectiveFovLH(tmp2, (60.0f * 3.14159f) / 180.0f, aspect, 0.1f, 100.0f);
	MTLMatrixMultiply(udata + 16, tmp1, tmp2); // viewproj
	
	MTLVec4Set(udata + 32, grid_width * 0.5f, 10, grid_depth * 0.5f, 1); // lightPos
	MTLVec4Set(udata + 36, eye[0], eye[1], eye[2], 1.0f); // eyePos
	
	time += elapsedtime;
	lasttime += elapsedtime;
	
	// render teapot
	renderencoder = [commandbuffer renderCommandEncoderWithDescriptor:renderpassdesc];
	
	[renderencoder setCullMode:MTLCullModeBack];
	[renderencoder setRenderPipelineState:renderstate];
	[renderencoder setDepthStencilState:depthstate];
	[renderencoder setVertexBuffer:renderuniforms[currentframe] offset:0 atIndex:1];
	[renderencoder setFragmentBuffer:materials offset:0 atIndex:0];
	[renderencoder setFragmentTexture:texture atIndex:0];
	
	for (unsigned int i = 0; i < num_logical_batches; ++i) {
		[renderencoder setVertexBuffer:batch_physcical_vbo
								offset:i * logical_batch_vbo_size
							   atIndex:0];
		
		[renderencoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								  indexCount:teapots_per_logical_batch * teapotindices
								   indexType:MTLIndexTypeUInt16
								 indexBuffer:batch_physcical_ibo
						   indexBufferOffset:i * logical_batch_ibo_size];
	}
	
	[renderencoder endEncoding];

	// submit
	[commandbuffer presentDrawable:drawable];
	
#if NUM_QUEUED_FRAMES > 1
	__block dispatch_semaphore_t block_sema = framesema;
	
	[commandbuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
		// increment semaphore
		dispatch_semaphore_signal(block_sema);
	}];
#endif
	
	[commandbuffer commit];
	
	currentframe = (currentframe + 1) % NUM_QUEUED_FRAMES;
	
	if (lasttime > 1.0) {
		lasttime	= fmodf(lasttime, 1.0f);
		framerate	= (int)(1.0f / elapsedtime);
		frametime	= (int)(elapsedtime * 1000.0f);

		UpdateLabels(view);
	}
}

void FreeSomeMemory()
{
	depthtex = nil;
}

