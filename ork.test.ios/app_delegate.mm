////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import "app_delegate.h"
#import "test_list_view_controller.h"

#include <ork/util/logger.h>
#include <ork/util/logger_ios_ui.h>

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

    // Create test list view controller as root
    TestListViewController *testListVC = [[TestListViewController alloc] init];
    UINavigationController *navController = [[UINavigationController alloc] initWithRootViewController:testListVC];

    self.window.rootViewController = navController;
    [self.window makeKeyAndVisible];

    return YES;
}

@end
