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
	NSMutableArray*	_labels;
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
		self.contentScaleFactor = [UIScreen mainScreen].nativeScale;
		
		_metalLayer	= (CAMetalLayer*)self.layer;
		_device		= MTLCreateSystemDefaultDevice();
		
		_metalLayer.device			= _device;
		_metalLayer.pixelFormat		= MTLPixelFormatBGRA8Unorm_sRGB;
		_metalLayer.framebufferOnly	= YES;
		
		_labels = [[NSMutableArray alloc] initWithCapacity:5];
	}
	
	return self;
}

- (void)setContentScaleFactor:(CGFloat)contentScaleFactor
{
	[super setContentScaleFactor:contentScaleFactor];
	_layerSizeDidUpdate = YES;
}

- (void)layoutSubviews
{
	[super layoutSubviews];
	_layerSizeDidUpdate = YES;
}

- (NSUInteger)addLabel
{
	NSUInteger ret = [_labels count];
	UILabel* label = [[UILabel alloc] init];
	
	[label setBackgroundColor:[UIColor blackColor]];
	[label setTextColor:[UIColor whiteColor]];
	[label setTextAlignment:NSTextAlignmentLeft];
	[label setAutoresizingMask:UIViewAutoresizingNone];
	
	[_labels addObject:label];
	
	[self addSubview:label];
	[self setLabelText:ret text:@"empty"];
	
	return ret;
}

- (void)setLabelText:(NSUInteger)index text:(NSString*)text
{
	UILabel* label = [_labels objectAtIndex:index];
	
	[label setText:text];
	[label sizeToFit];
	
	CGRect frame = [label frame];
	
	frame.origin.x = 15;
	frame.origin.y = 15 + index * (frame.size.height + 10);
	
	[label setFrame:frame];
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
		if (_layerSizeDidUpdate) {
			CGSize drawsize = self.bounds.size;
			
			drawsize.width *= self.contentScaleFactor;
			drawsize.height *= self.contentScaleFactor;
			
			_metalLayer.drawableSize = drawsize;
			_layerSizeDidUpdate = NO;
		}
		
		Render(self, 0, frametime);
		_currentDrawable = nil;
	}
}

@end
