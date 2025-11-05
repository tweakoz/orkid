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
void runCurlTests();
void runLZ4Tests();
void runRapidJSONTests();
void runNlohmannTests();

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

    // Create test button grid (2 columns)
    _testButtonStack = [[UIStackView alloc] init];
    _testButtonStack.axis = UILayoutConstraintAxisVertical;
    _testButtonStack.spacing = 10;
    _testButtonStack.alignment = UIStackViewAlignmentFill;
    _testButtonStack.distribution = UIStackViewDistributionFill;
    [_mainStack addArrangedSubview:_testButtonStack];

    // Row 1: Math and Dataflow
    UIStackView *testRow1 = [[UIStackView alloc] init];
    testRow1.axis = UILayoutConstraintAxisHorizontal;
    testRow1.spacing = 10;
    testRow1.alignment = UIStackViewAlignmentFill;
    testRow1.distribution = UIStackViewDistributionFillEqually;
    [testRow1.heightAnchor constraintEqualToConstant:50].active = YES;
    [_testButtonStack addArrangedSubview:testRow1];

    UIButton *mathBtn = [self createTestButton:@"Math" selector:@selector(runMathTestsTapped)];
    UIButton *dataflowBtn = [self createTestButton:@"Dataflow" selector:@selector(runDataflowTestsTapped)];
    [testRow1 addArrangedSubview:mathBtn];
    [testRow1 addArrangedSubview:dataflowBtn];

    // Row 2: Kernel and Curl
    UIStackView *testRow2 = [[UIStackView alloc] init];
    testRow2.axis = UILayoutConstraintAxisHorizontal;
    testRow2.spacing = 10;
    testRow2.alignment = UIStackViewAlignmentFill;
    testRow2.distribution = UIStackViewDistributionFillEqually;
    [testRow2.heightAnchor constraintEqualToConstant:50].active = YES;
    [_testButtonStack addArrangedSubview:testRow2];

    UIButton *kernelBtn = [self createTestButton:@"Kernel" selector:@selector(runKernelTestsTapped)];
    UIButton *curlBtn = [self createTestButton:@"Curl" selector:@selector(runCurlTestsTapped)];
    [testRow2 addArrangedSubview:kernelBtn];
    [testRow2 addArrangedSubview:curlBtn];

    // Row 3: LZ4 and RapidJSON
    UIStackView *testRow3 = [[UIStackView alloc] init];
    testRow3.axis = UILayoutConstraintAxisHorizontal;
    testRow3.spacing = 10;
    testRow3.alignment = UIStackViewAlignmentFill;
    testRow3.distribution = UIStackViewDistributionFillEqually;
    [testRow3.heightAnchor constraintEqualToConstant:50].active = YES;
    [_testButtonStack addArrangedSubview:testRow3];

    UIButton *lz4Btn = [self createTestButton:@"LZ4" selector:@selector(runLZ4TestsTapped)];
    UIButton *jsonBtn = [self createTestButton:@"RapidJSON" selector:@selector(runRapidJSONTestsTapped)];
    [testRow3 addArrangedSubview:lz4Btn];
    [testRow3 addArrangedSubview:jsonBtn];

    // Row 4: Nlohmann and spacer
    UIStackView *testRow4 = [[UIStackView alloc] init];
    testRow4.axis = UILayoutConstraintAxisHorizontal;
    testRow4.spacing = 10;
    testRow4.alignment = UIStackViewAlignmentFill;
    testRow4.distribution = UIStackViewDistributionFillEqually;
    [testRow4.heightAnchor constraintEqualToConstant:50].active = YES;
    [_testButtonStack addArrangedSubview:testRow4];

    UIButton *nlohmannBtn = [self createTestButton:@"Nlohmann" selector:@selector(runNlohmannTestsTapped)];
    UIView *spacer = [[UIView alloc] init];
    [testRow4 addArrangedSubview:nlohmannBtn];
    [testRow4 addArrangedSubview:spacer];

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

- (UIButton*)createTestButton:(NSString*)title selector:(SEL)selector {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont systemFontOfSize:14];
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    button.backgroundColor = [UIColor colorWithRed:0.2 green:0.4 blue:0.8 alpha:1.0];
    button.layer.cornerRadius = 8;
    [button addTarget:self action:selector forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)addLogChannelButton:(NSString*)channelName withColor:(UIColor*)color viewController:(UIViewController*)vc {
    // Create button and store view controller on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        // Don't create duplicate buttons (check on main thread)
        if (self.channelButtons[channelName]) {
            return;
        }

        // Store view controller (on main thread)
        self.channelViewControllers[channelName] = vc;

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

    // Create a vertical stack to hold rows
    UIStackView *verticalStack = [[UIStackView alloc] init];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = 10;
    verticalStack.alignment = UIStackViewAlignmentFill;
    verticalStack.distribution = UIStackViewDistributionFill;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [_logChannelGrid addSubview:verticalStack];

    // Pin vertical stack to grid
    [NSLayoutConstraint activateConstraints:@[
        [verticalStack.topAnchor constraintEqualToAnchor:_logChannelGrid.topAnchor],
        [verticalStack.leadingAnchor constraintEqualToAnchor:_logChannelGrid.leadingAnchor],
        [verticalStack.trailingAnchor constraintEqualToAnchor:_logChannelGrid.trailingAnchor],
        [verticalStack.bottomAnchor constraintEqualToAnchor:_logChannelGrid.bottomAnchor]
    ]];

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
            [currentRow.heightAnchor constraintEqualToConstant:50].active = YES;
            [verticalStack addArrangedSubview:currentRow];
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

    // Restore highlights for unseen channels
    for (NSString *unseenChannel in _unseenChannels) {
        UIButton *button = _channelButtons[unseenChannel];
        if (button) {
            button.layer.borderWidth = 1.5;
            button.layer.borderColor = [UIColor yellowColor].CGColor;
        }
    }
}

- (void)highlightLogChannel:(NSString*)channelName {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Always mark as unseen, even if button doesn't exist yet
        [self.unseenChannels addObject:channelName];

        // Apply highlight to button if it exists
        UIButton *button = self.channelButtons[channelName];
        if (button) {
            button.layer.borderWidth = 1.5;
            button.layer.borderColor = [UIColor yellowColor].CGColor;
        }
        // If button doesn't exist yet, it will be highlighted when created in rebuildLogChannelGrid
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

- (void)runCurlTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runCurlTests();
    });
}

- (void)runLZ4TestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runLZ4Tests();
    });
}

- (void)runRapidJSONTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runRapidJSONTests();
    });
}

- (void)runNlohmannTestsTapped {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runNlohmannTests();
    });
}

@end
