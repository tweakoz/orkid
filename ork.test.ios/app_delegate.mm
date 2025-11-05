////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import "app_delegate.h"
#import "main_view_controller.h"

#include <ork/util/logger.h>
#include <ork/util/logger_ios_ui.h>
#include <ork/ios/app_init.h>

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Initialize Orkid logger with iOS UI backend
    auto logger_backend = ork::logger()->_backend;
    if (logger_backend) {
        ork::installIOSUIToBackend(logger_backend.get());
    }

    // Create window
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.backgroundColor = [UIColor blackColor];

    // Create main view controller
    MainViewController *mainVC = [[MainViewController alloc] init];
    UINavigationController *navController = [[UINavigationController alloc] initWithRootViewController:mainVC];

    self.window.rootViewController = navController;
    [self.window makeKeyAndVisible];

    // Connect logger to main view controller
    ork::setIOSUIMainViewController((__bridge void*)mainVC);

    // Setup display link for per-frame polling
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    return YES;
}

- (void)onFrame:(CADisplayLink *)displayLink {
    // Poll Orkid core every frame on main thread
    _coreapppoll();
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Stop display link
    [self.displayLink invalidate];
    self.displayLink = nil;

    // Shutdown Orkid core
    _coreappexit();
}

@end
