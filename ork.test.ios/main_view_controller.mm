////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import "main_view_controller.h"
#include <ork/util/logger.h>

// Test runner functions
void runMathTests();
void runDataflowTests();
void runKernelTests();

@interface MainViewController ()
@property (strong, nonatomic) UIScrollView *scrollView;
@property (strong, nonatomic) UIStackView *mainStack;
@property (strong, nonatomic) UIStackView *testButtonStack;
@property (strong, nonatomic) UIView *logChannelGrid;
@property (strong, nonatomic) NSMutableDictionary<NSString*, UIButton*> *channelButtons;
@property (strong, nonatomic) NSMutableDictionary<NSString*, UIViewController*> *channelViewControllers;
@property (strong, nonatomic) NSMutableSet<NSString*> *unseenChannels;
@end

@implementation MainViewController

- (instancetype)init {
    self = [super init];
    if (self) {
        self.title = @"Orkid Tests";
        _channelButtons = [[NSMutableDictionary alloc] init];
        _channelViewControllers = [[NSMutableDictionary alloc] init];
        _unseenChannels = [[NSMutableSet alloc] init];
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];

    // Create scroll view for entire content
    _scrollView = [[UIScrollView alloc] initWithFrame:self.view.bounds];
    _scrollView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_scrollView];

    // Main vertical stack
    _mainStack = [[UIStackView alloc] init];
    _mainStack.axis = UILayoutConstraintAxisVertical;
    _mainStack.spacing = 20;
    _mainStack.alignment = UIStackViewAlignmentFill;
    _mainStack.distribution = UIStackViewDistributionFill;
    _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
    [_scrollView addSubview:_mainStack];

    // Section 1: Test Runners
    UILabel *testLabel = [[UILabel alloc] init];
    testLabel.text = @"Test Runners";
    testLabel.font = [UIFont boldSystemFontOfSize:18];
    testLabel.textColor = [UIColor whiteColor];
    testLabel.textAlignment = NSTextAlignmentCenter;
    [_mainStack addArrangedSubview:testLabel];

    _testButtonStack = [[UIStackView alloc] init];
    _testButtonStack.axis = UILayoutConstraintAxisVertical;
    _testButtonStack.spacing = 10;
    _testButtonStack.alignment = UIStackViewAlignmentFill;
    _testButtonStack.distribution = UIStackViewDistributionFillEqually;
    [_mainStack addArrangedSubview:_testButtonStack];

    // Add test buttons
    [self addTestButton:@"Math Tests" selector:@selector(runMathTestsTapped)];
    [self addTestButton:@"Dataflow Tests" selector:@selector(runDataflowTestsTapped)];
    [self addTestButton:@"Kernel Tests" selector:@selector(runKernelTestsTapped)];

    // Section 2: Log Channels
    UILabel *logLabel = [[UILabel alloc] init];
    logLabel.text = @"Log Channels";
    logLabel.font = [UIFont boldSystemFontOfSize:18];
    logLabel.textColor = [UIColor whiteColor];
    logLabel.textAlignment = NSTextAlignmentCenter;
    [_mainStack addArrangedSubview:logLabel];

    // Container for log channel grid (will be populated dynamically)
    _logChannelGrid = [[UIView alloc] init];
    _logChannelGrid.translatesAutoresizingMaskIntoConstraints = NO;
    [_mainStack addArrangedSubview:_logChannelGrid];

    // Layout constraints
    [NSLayoutConstraint activateConstraints:@[
        [_mainStack.topAnchor constraintEqualToAnchor:_scrollView.topAnchor constant:20],
        [_mainStack.leadingAnchor constraintEqualToAnchor:_scrollView.leadingAnchor constant:20],
        [_mainStack.trailingAnchor constraintEqualToAnchor:_scrollView.trailingAnchor constant:-20],
        [_mainStack.bottomAnchor constraintEqualToAnchor:_scrollView.bottomAnchor constant:-20],
        [_mainStack.widthAnchor constraintEqualToAnchor:_scrollView.widthAnchor constant:-40],
    ]];
}

- (void)addTestButton:(NSString*)title selector:(SEL)selector {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont boldSystemFontOfSize:16];
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    button.backgroundColor = [UIColor colorWithRed:0.2 green:0.4 blue:0.8 alpha:1.0];
    button.layer.cornerRadius = 8;
    [button addTarget:self action:selector forControlEvents:UIControlEventTouchUpInside];
    [button setContentEdgeInsets:UIEdgeInsetsMake(12, 20, 12, 20)];
    [_testButtonStack addArrangedSubview:button];
}

- (void)addLogChannelButton:(NSString*)channelName withColor:(UIColor*)color viewController:(UIViewController*)vc {
    // Store view controller
    _channelViewControllers[channelName] = vc;

    // Don't create duplicate buttons
    if (_channelButtons[channelName]) {
        return;
    }

    // Create button on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
        [button setTitle:channelName forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont systemFontOfSize:14];
        [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
        button.backgroundColor = [color colorWithAlphaComponent:0.3];
        button.layer.cornerRadius = 8;
        button.layer.borderWidth = 0;
        button.layer.borderColor = [UIColor clearColor].CGColor;
        [button setContentEdgeInsets:UIEdgeInsetsMake(10, 15, 10, 15)];

        // Tag button with channel name
        button.accessibilityIdentifier = channelName;

        [button addTarget:self action:@selector(logChannelTapped:) forControlEvents:UIControlEventTouchUpInside];

        self.channelButtons[channelName] = button;

        // Rebuild grid layout
        [self rebuildLogChannelGrid];
    });
}

- (void)rebuildLogChannelGrid {
    // Remove all subviews
    for (UIView *subview in _logChannelGrid.subviews) {
        [subview removeFromSuperview];
    }

    // Create grid with 2 columns
    NSArray *channelNames = [_channelButtons.allKeys sortedArrayUsingSelector:@selector(compare:)];

    UIStackView *currentRow = nil;
    int col = 0;

    for (NSString *channelName in channelNames) {
        if (col == 0) {
            // Start new row
            currentRow = [[UIStackView alloc] init];
            currentRow.axis = UILayoutConstraintAxisHorizontal;
            currentRow.spacing = 10;
            currentRow.alignment = UIStackViewAlignmentFill;
            currentRow.distribution = UIStackViewDistributionFillEqually;
            currentRow.translatesAutoresizingMaskIntoConstraints = NO;
            [_logChannelGrid addSubview:currentRow];
        }

        UIButton *button = _channelButtons[channelName];
        [currentRow addArrangedSubview:button];

        col++;
        if (col >= 2) {
            col = 0;
        }
    }

    // Add spacer to last row if only 1 button
    if (col == 1 && currentRow) {
        UIView *spacer = [[UIView alloc] init];
        [currentRow addArrangedSubview:spacer];
    }

    // Layout rows vertically
    CGFloat yOffset = 0;
    for (UIView *row in _logChannelGrid.subviews) {
        [NSLayoutConstraint activateConstraints:@[
            [row.topAnchor constraintEqualToAnchor:_logChannelGrid.topAnchor constant:yOffset],
            [row.leadingAnchor constraintEqualToAnchor:_logChannelGrid.leadingAnchor],
            [row.trailingAnchor constraintEqualToAnchor:_logChannelGrid.trailingAnchor],
            [row.heightAnchor constraintEqualToConstant:50]
        ]];
        yOffset += 60; // 50 height + 10 spacing
    }

    // Set grid height
    [_logChannelGrid.heightAnchor constraintEqualToConstant:yOffset].active = YES;
}

- (void)highlightLogChannel:(NSString*)channelName {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIButton *button = self.channelButtons[channelName];
        if (button) {
            // Mark as unseen
            [self.unseenChannels addObject:channelName];

            // Highlight with yellow border
            button.layer.borderWidth = 3;
            button.layer.borderColor = [UIColor yellowColor].CGColor;
        }
    });
}

- (void)logChannelTapped:(UIButton*)sender {
    NSString *channelName = sender.accessibilityIdentifier;
    UIViewController *vc = _channelViewControllers[channelName];

    if (vc) {
        // Mark as seen
        [_unseenChannels removeObject:channelName];

        // Remove highlight
        sender.layer.borderWidth = 0;
        sender.layer.borderColor = [UIColor clearColor].CGColor;

        // Push to navigation
        [self.navigationController pushViewController:vc animated:YES];
    }
}

- (void)runMathTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runMathTests();
    });
}

- (void)runDataflowTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runDataflowTests();
    });
}

- (void)runKernelTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runKernelTests();
    });
}

@end
