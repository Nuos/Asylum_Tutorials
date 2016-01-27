//
//  MetalView.m
//  70_Metal
//
//  Created by iszennai on 21/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#import "MetalView.h"

extern void Render(MetalView* view, float alpha, float elapsedtime);

@implementation MetalView
{
@private
	__weak CAMetalLayer* _metalLayer;
	BOOL _layerSizeDidUpdate;
	
	id<MTLTexture>	_depthTex;
	id<MTLTexture>	_stencilTex;
	id<MTLTexture>	_colorTex;
}

@synthesize currentDrawable = _currentDrawable;

+ (Class)layerClass
{
	return [CAMetalLayer class];
}

- (instancetype)initWithCoder:(NSCoder*)coder
{
	self = [super initWithCoder:coder];
	
	if (self) {
		self.opaque = YES;
		self.backgroundColor = nil;
		
		_metalLayer	= (CAMetalLayer*)self.layer;
		_device		= MTLCreateSystemDefaultDevice();
		
		_metalLayer.device			= _device;
		_metalLayer.pixelFormat		= MTLPixelFormatBGRA8Unorm;
		_metalLayer.framebufferOnly	= YES;
	}
	
	return self;
}

- (id<CAMetalDrawable>)currentDrawable
{
	if (_currentDrawable == nil)
		_currentDrawable = [_metalLayer nextDrawable];
	
	return _currentDrawable;
}

- (void)display:(float)frametime
{
	@autoreleasepool
	{
		Render(self, 0, frametime);
		
		_currentDrawable = nil;
	}
}

@end
