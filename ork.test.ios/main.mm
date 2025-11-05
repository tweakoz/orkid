////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import <UIKit/UIKit.h>
#import "app_delegate.h"
#include <ork/ios/app_init.h>

int main(int argc, char * argv[]) {
    @autoreleasepool {
        // Initialize Orkid core before UI starts
        _coreappinit(argc, argv);

        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
