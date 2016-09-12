//
//  MetalView.h
//  70_Metal
//
//  Created by iszennai on 21/01/16.
//  Copyright © 2016 Asylum. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>

@interface MetalView : UIView

@property(nonatomic, readonly) id<MTLDevice> device;
@property(nonatomic, readonly) id<CAMetalDrawable> currentDrawable;

- (NSUInteger)addLabel;
- (void)setLabelText:(NSUInteger)index text:(NSString*)text;
- (void)display:(float)frametime;

@end

