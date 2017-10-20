
#import "GLView.h"

#include <OpenGL/gl3.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/glext.h>
#include <string>

extern unsigned int screenwidth;
extern unsigned int screenheight;

extern bool InitScene();
extern void UninitScene();
extern void Render(float, float);

std::string GetResource(const std::string& file)
{
	std::string name, ext;
	size_t loc1 = file.find_last_of('.');
	size_t loc2 = file.find_last_of('/');
	
	assert(loc1 != std::string::npos);
	
	if (loc2 != std::string::npos)
		name = file.substr(loc2 + 1, loc1 - loc2 - 1);
	else
		name = file.substr(0, loc1);
	
	ext = file.substr(loc1 + 1);
	
	NSString* path = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:name.c_str()] ofType:[NSString stringWithUTF8String:ext.c_str()]];
	return std::string([path UTF8String]);
}

@implementation GLView

- (id)initWithCoder:(NSCoder *)coder
{
	self = [super initWithCoder:coder];
	
	if (self) {
		NSOpenGLPixelFormatAttribute attributes[] = {
			NSOpenGLPFAColorSize, 24,
			NSOpenGLPFAAlphaSize, 8,
			NSOpenGLPFADoubleBuffer,
			NSOpenGLPFAAccelerated,
			NSOpenGLPFANoRecovery,
			NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
			0
		};
		
		NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
		
		if (context == nil) {
			attributes[7] = (NSOpenGLPixelFormatAttribute)0;
			
			format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
			context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
		}
		
		[self setOpenGLContext:context];
		[context makeCurrentContext];
		
		screenwidth = self.bounds.size.width;
		screenheight = self.bounds.size.height;
		
		InitScene();
	}
	
	return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
	Render(0, 0);
	
	[self.openGLContext flushBuffer];
}

@end
