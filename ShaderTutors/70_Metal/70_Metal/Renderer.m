//
//  Renderer.m
//  70_Metal
//
//  Created by iszennai on 21/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#import "MetalView.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

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
id<MTLTexture>				texture;
id<MTLTexture>				depthtex;

MTLRenderPassDescriptor*	renderpassdesc;
MetalMesh*					mesh;
AVAudioPlayer*				player;
dispatch_semaphore_t		framesema;
int							currentframe = 0;

float MTLVec3Dot(const float a[3], const float b[3]);
float MTLVec3Length(const float a[3]);

void MTLVec3Set(float out[3], float x, float y, float z);
void MTLVec4Set(float out[4], float x, float y, float z, float w);
void MTLVec3Normalize(float out[3], const float v[3]);
void MTLVec3Cross(float out[3], const float a[3], const float b[3]);
void MTLMatrixIdentity(float out[16]);
void MTLMatrixLookAtRH(float out[16], const float eye[3], const float look[3], const float up[3]);
void MTLMatrixPerspectiveRH(float out[16], float fovy, float aspect, float nearplane, float farplane);
void MTLMatrixMultiply(float out[16], const float a[16], const float b[16]);
void MTLMatrixRotationAxis(float out[16], float angle, float x, float y, float z);

// *****************************************************************************************************************************
//
// MetalMesh class
//
// *****************************************************************************************************************************

@interface MetalMesh : NSObject

@property(nonatomic, readonly, strong) id<MTLBuffer> vertexBuffer;
@property(nonatomic, readonly, strong) id<MTLBuffer> indexBuffer;
@property(readonly) NSUInteger numIndices;

+ (MetalMesh*)loadFromQM:(NSString*)file;

- (id)initWithBuffers:(id<MTLBuffer>)vertexbuffer indexBuffer:(id<MTLBuffer>)indexbuffer indexCount:(NSUInteger)indexcount indexStride:(NSUInteger)indexstride;

@end

@implementation MetalMesh

@synthesize vertexBuffer = _vertexBuffer;
@synthesize indexBuffer = _indexBuffer;
@synthesize numIndices = _numIndices;

+ (MetalMesh*)loadFromQM:(NSString*)file
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
	
	id<MTLBuffer> vbo = [mtldevice newBufferWithBytes:vdata length:numvertices * vstride options:MTLResourceOptionCPUCacheModeDefault];
	free(vdata);
	
	void* idata = malloc(numindices * istride);
	fread(idata, istride, numindices, infile);
	
	id<MTLBuffer> ibo = [mtldevice newBufferWithBytes:idata length:numindices * istride options:MTLResourceOptionCPUCacheModeDefault];
	free(idata);
	
	// skip subset and material info
	fclose(infile);
	
	return [[MetalMesh alloc] initWithBuffers:vbo indexBuffer:ibo indexCount:numindices indexStride:istride];
}

- (id)initWithBuffers:(id<MTLBuffer>)vertexbuffer indexBuffer:(id<MTLBuffer>)indexbuffer indexCount:(NSUInteger)indexcount indexStride:(NSUInteger)indexstride
{
	self = [super init];
	
	if (self) {
		_vertexBuffer = vertexbuffer;
		_indexBuffer = indexbuffer;
		_numIndices = indexcount;
	}
	
	return self;
}

- (void)dealloc
{
	_vertexBuffer = nil;
	_indexBuffer = nil;
}

@end

// *****************************************************************************************************************************
//
// Sample impl
//
// *****************************************************************************************************************************

BOOL InitScene(MetalView* view)
{
	framesema = dispatch_semaphore_create(NUM_QUEUED_FRAMES);
	
	mtldevice = [view device];
	commandqueue = [mtldevice newCommandQueue];
	defaultlibrary = [mtldevice newDefaultLibrary];

	mesh = [MetalMesh loadFromQM:[[NSBundle mainBundle] pathForResource:@"teapot" ofType:@"qm"]];
	
	if (!mesh) {
		NSLog(@"Error: Could not load mesh!");
		return NO;
	}
	
	MTLRenderPipelineDescriptor* pipelinedesc = [[MTLRenderPipelineDescriptor alloc] init];
	
	pipelinedesc.vertexFunction = [defaultlibrary newFunctionWithName:@"vs_main"];
	pipelinedesc.fragmentFunction = [defaultlibrary newFunctionWithName:@"ps_main"];
	pipelinedesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
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
	
	MTLTextureDescriptor* texdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:128 height:128 mipmapped:NO];
	texture = [mtldevice newTextureWithDescriptor:texdesc];
	
	NSURL* musicurl = [[NSURL alloc] initFileURLWithPath:[[NSBundle mainBundle] pathForResource:@"breakingthelaw" ofType:@"mp3"]];
	player = [[AVAudioPlayer alloc] initWithContentsOfURL:musicurl fileTypeHint:AVFileTypeMPEGLayer3 error:nil];
	
	player.numberOfLoops = 0; // play once
	
	return YES;
}

void UninitScene()
{
	[player stop];
	player = nil;
	
	for (int i = 0; i < NUM_QUEUED_FRAMES; ++i) {
		computeuniforms[i] = nil;
		renderuniforms[i] = nil;
	}
	
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
	static BOOL wasplaying = NO;
	
	BOOL isplaying = [player isPlaying];
	
	if (wasplaying && !isplaying) {
		lasttime = time;
	} else if (time - lasttime > 2.0f && !isplaying) {
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
	colorattachment0.clearColor = MTLClearColorMake(0.0f, 0.125f, 0.3f, 1.0f);
	
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
	float eye[3] = { 0, 0, 3 };
	float look[3] = { 0, 0, 0 };
	float up[3] = { 0, 1, 0 };
	
	float tmp1[16];
	float tmp2[16];
	
	float* udata = [renderuniforms[currentframe] contents];
	float aspect = [view bounds].size.width / [view bounds].size.height;
	
	MTLMatrixIdentity(tmp2);
	
	tmp2[12] = -0.108f;
	tmp2[13] = -0.7875f;
	
	MTLMatrixRotationAxis(tmp1, fmodf(time * 20.0f, 360.0f) * (3.14159f / 180.0f), 1, 0, 0);
	MTLMatrixMultiply(udata, tmp2, tmp1);
	
	MTLMatrixRotationAxis(tmp2, fmodf(time * 20.0f, 360.0f) * (3.14159f / 180.0f), 0, 1, 0);
	MTLMatrixMultiply(udata, udata, tmp2);
	
	MTLMatrixLookAtRH(tmp1, eye, look, up);
	MTLMatrixPerspectiveRH(tmp2, (60.0f * 3.14159f) / 180.0f, aspect, 0.1f, 100.0f);
	MTLMatrixMultiply(udata + 16, tmp1, tmp2);
	
	MTLVec4Set(udata + 32, 6, 3, 10, 1);
	MTLVec4Set(udata + 36, eye[0], eye[1], eye[2], 1.0f);
	
	time += elapsedtime;
	
	// render teapot
	renderencoder = [commandbuffer renderCommandEncoderWithDescriptor:renderpassdesc];
	
	[renderencoder setRenderPipelineState:renderstate];
	[renderencoder setDepthStencilState:depthstate];
	[renderencoder setVertexBuffer:mesh.vertexBuffer offset:0 atIndex:0];
	[renderencoder setVertexBuffer:renderuniforms[currentframe] offset:0 atIndex:1];
	[renderencoder setFragmentTexture:texture atIndex:0];
	[renderencoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:mesh.numIndices indexType:MTLIndexTypeUInt16 indexBuffer:mesh.indexBuffer indexBufferOffset:0];
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
}

void FreeSomeMemory()
{
	depthtex = nil;
}

// *****************************************************************************************************************************
//
// Math functions
//
// *****************************************************************************************************************************

float MTLVec3Dot(const float a[3], const float b[3])
{
	return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
}

float MTLVec3Length(const float a[3])
{
	return sqrtf(MTLVec3Dot(a, a));
}

void MTLVec3Set(float out[3], float x, float y, float z)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

void MTLVec4Set(float out[4], float x, float y, float z, float w)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

void MTLVec3Normalize(float out[3], const float v[3])
{
	float il = 1.0f / MTLVec3Length(v);
	
	out[0] = v[0] * il;
	out[1] = v[1] * il;
	out[2] = v[2] * il;
}

void MTLVec3Cross(float out[3], const float a[3], const float b[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

void MTLMatrixIdentity(float out[16])
{
	memset(out, 0, 16 * sizeof(float));
	out[0] = out[5] = out[10] = out[15] = 1;
}

void MTLMatrixLookAtRH(float out[16], const float eye[3], const float look[3], const float up[3])
{
	float x[3], y[3], z[3];
	
	z[0] = eye[0] - look[0];
	z[1] = eye[1] - look[1];
	z[2] = eye[2] - look[2];
	
	MTLVec3Normalize(z, z);
	MTLVec3Cross(x, up, z);
	
	MTLVec3Normalize(x, x);
	MTLVec3Cross(y, z, x);
	
	out[0] = x[0];		out[1] = y[0];		out[2] = z[0];		out[3] = 0.0f;
	out[4] = x[1];		out[5] = y[1];		out[6] = z[1];		out[7] = 0.0f;
	out[8] = x[2];		out[9] = y[2];		out[10] = z[2];		out[11] = 0.0f;
	
	out[12] = -MTLVec3Dot(x, eye);
	out[13] = -MTLVec3Dot(y, eye);
	out[14] = -MTLVec3Dot(z, eye);
	out[15] = 1.0f;
}

void MTLMatrixPerspectiveRH(float out[16], float fovy, float aspect, float nearplane, float farplane)
{
	out[5] = 1.0f / tanf(fovy / 2);
	out[0] = out[5] / aspect;
	
	out[1] = out[2] = out[3] = 0;
	out[4] = out[6] = out[7] = 0;
	out[8] = out[9] = 0;
	out[12] = out[13] = out[15] = 0;
	
	out[11] = -1.0f;
	
	out[10] = (farplane + nearplane) / (nearplane - farplane);
	out[14] = 2 * farplane * nearplane / (nearplane - farplane);
}

void MTLMatrixMultiply(float out[16], const float a[16], const float b[16])
{
	float tmp[16];
	
	tmp[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
	tmp[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
	tmp[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
	tmp[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];
	
	tmp[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
	tmp[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
	tmp[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
	tmp[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];
	
	tmp[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
	tmp[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
	tmp[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
	tmp[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];
	
	tmp[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
	tmp[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
	tmp[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
	tmp[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
	
	memcpy(out, tmp, 16 * sizeof(float));
}

void MTLMatrixRotationAxis(float out[16], float angle, float x, float y, float z)
{
	float u[3] = { x, y, z };
	
	float cosa = cosf(angle);
	float sina = sinf(angle);
	
	MTLVec3Normalize(u, u);
	
	out[0] = cosa + u[0] * u[0] * (1.0f - cosa);
	out[1] = u[0] * u[1] * (1.0f - cosa) - u[2] * sina;
	out[2] = - (u[0] * u[2] * (1.0f - cosa) + u[1] * sina);
	out[3] = 0;
	
	out[4] = u[1] * u[0] * (1.0f - cosa) + u[2] * sina;
	out[5] = cosa + u[1] * u[1] * (1.0f - cosa);
	out[6] = - (u[1] * u[2] * (1.0f - cosa) - u[0] * sina);
	out[7] = 0;
	
	out[8] = - (u[2] * u[0] * (1.0f - cosa) - u[1] * sina);
	out[9] = - (u[2] * u[1] * (1.0f - cosa) + u[0] * sina);
	out[10] = cosa + u[2] * u[2] * (1.0f - cosa);
	out[11] = 0;
	
	out[12] = out[13] = out[14] = 0;
	out[15] = 1;
}
