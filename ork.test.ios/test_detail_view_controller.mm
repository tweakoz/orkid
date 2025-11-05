////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import "test_detail_view_controller.h"
#include <ork/test/ork_tests.h>

#include <ork/util/logger.h>
#include <ork/util/logger_ios_ui.h>

@interface TestDetailViewController ()
@property (nonatomic, strong) OrkTestInfo* testInfo;
@property (nonatomic, strong) UIViewController* loggerViewController;
@property (nonatomic, strong) UIButton* runButton;
@property (nonatomic, assign) BOOL testRunning;
@end

@implementation TestDetailViewController

- (instancetype)initWithTest:(OrkTestInfo*)testInfo {
    self = [super init];
    if (self) {
        _testInfo = testInfo;
        _testRunning = NO;
        self.title = testInfo.name;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];

    // Get the logger root view controller
    _loggerViewController = ork::getIOSLoggerRootViewController();

    if (_loggerViewController) {
        // Embed logger view controller
        [self addChildViewController:_loggerViewController];
        _loggerViewController.view.frame = self.view.bounds;
        _loggerViewController.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        [self.view addSubview:_loggerViewController.view];
        [_loggerViewController didMoveToParentViewController:self];
    }

    // Create run button at bottom
    _runButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _runButton.frame = CGRectMake(0, self.view.bounds.size.height - 60, self.view.bounds.size.width, 60);
    _runButton.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
    [_runButton setTitle:@"Run Test" forState:UIControlStateNormal];
    _runButton.titleLabel.font = [UIFont boldSystemFontOfSize:18.0f];
    _runButton.backgroundColor = [UIColor colorWithRed:0.2f green:0.6f blue:0.9f alpha:1.0f];
    [_runButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [_runButton addTarget:self action:@selector(runTestTapped:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_runButton];

    // Adjust logger view to not overlap button
    if (_loggerViewController) {
        CGRect frame = _loggerViewController.view.frame;
        frame.size.height = self.view.bounds.size.height - 60;
        _loggerViewController.view.frame = frame;
    }
}

- (void)runTestTapped:(UIButton*)sender {
    if (_testRunning) {
        return;
    }

    _testRunning = YES;
    [_runButton setTitle:@"Running..." forState:UIControlStateNormal];
    _runButton.enabled = NO;

    // Run test on background thread
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Execute the test
        if (self.testInfo.testFunction) {
            self.testInfo.testFunction();
        }

        // Re-enable button on main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.runButton setTitle:@"Run Test Again" forState:UIControlStateNormal];
            self.runButton.enabled = YES;
            self.testRunning = NO;
        });
    });
}

@end
