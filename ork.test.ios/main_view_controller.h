////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import <UIKit/UIKit.h>

@interface MainViewController : UIViewController

// Called when a new log channel is created
- (void)addLogChannelButton:(NSString*)channelName withColor:(UIColor*)color viewController:(UIViewController*)vc;

// Called when log channel receives new output
- (void)highlightLogChannel:(NSString*)channelName;

@end
