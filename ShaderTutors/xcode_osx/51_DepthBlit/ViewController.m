//
//  ViewController.m
//  51_DepthBlit
//
//  Created by István Szennai on 2017. 09. 07..
//  Copyright © 2017. Asylum. All rights reserved.
//

#import "ViewController.h"

@implementation ViewController
{
	NSTimer* _timer;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	_timer = [NSTimer timerWithTimeInterval:(1.0f / 60.0f) target:self selector:@selector(animationTimer:) userInfo:nil repeats:YES];
	
	[[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSDefaultRunLoopMode];
	[[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSEventTrackingRunLoopMode];
}


- (void)setRepresentedObject:(id)representedObject {
	[super setRepresentedObject:representedObject];

	// Update the view, if already loaded.
}


- (void)animationTimer:(NSTimer*)timer {
	[self.view setNeedsDisplay:YES];
}


@end
