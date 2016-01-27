//
//  MetalViewController.m
//  70_Metal
//
//  Created by iszennai on 21/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#import "MetalViewController.h"
#import "MetalView.h"

extern BOOL InitScene(MetalView* view);
extern void UninitScene();
extern void Update(float delta);
extern void FreeSomeMemory();

@implementation MetalViewController
{
@private
	CADisplayLink* _displayLink;
}

- (id)initWithCoder:(NSCoder *)coder
{
	self = [super initWithCoder:coder];
	
	if (self) {
		NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];

		[notificationCenter addObserver: self
							   selector: @selector(didEnterBackground:)
								   name: UIApplicationDidEnterBackgroundNotification
								 object: nil];
		
		[notificationCenter addObserver: self
							   selector: @selector(willEnterForeground:)
								   name: UIApplicationWillEnterForegroundNotification
								 object: nil];
	}
	
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver: self
													name: UIApplicationDidEnterBackgroundNotification
												  object: nil];
	
	[[NSNotificationCenter defaultCenter] removeObserver: self
													name: UIApplicationWillEnterForegroundNotification
												  object: nil];

	if (_displayLink)
		[_displayLink invalidate];
}

- (void)viewDidLoad
{
	[super viewDidLoad];
	
	assert([self.view isKindOfClass:[MetalView class]]);
	
	InitScene((MetalView*)self.view);
}

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];
	
	_displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self
													   selector:@selector(render)];
	_displayLink.frameInterval = 1;
	
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
					   forMode:NSDefaultRunLoopMode];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
	
	if (_displayLink)
		[_displayLink invalidate];
	
	UninitScene();
}

- (void)didEnterBackground:(NSNotification*)notification
{
	_displayLink.paused = YES;
	FreeSomeMemory();
}


- (void)willEnterForeground:(NSNotification*)notification
{
	_displayLink.paused = NO;
}

- (void)render
{
	static BOOL firstdraw = YES;
	static CFTimeInterval prevtime;
	
	if (firstdraw) {
		firstdraw = NO;
		prevtime = CACurrentMediaTime();
	}
	
	CFTimeInterval currtime = CACurrentMediaTime();
	CFTimeInterval elapsed = currtime - prevtime;
	
	Update([_displayLink duration]);
	[(MetalView*)self.view display:elapsed];
	
	prevtime = currtime;
}

@end
