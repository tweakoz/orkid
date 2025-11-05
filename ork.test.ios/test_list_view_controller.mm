////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import "test_list_view_controller.h"
#import "test_detail_view_controller.h"
#include <ork/test/ork_tests.h>

@interface TestListViewController ()
@property (nonatomic, strong) NSArray<OrkTestInfo*>* tests;
@end

@implementation TestListViewController

- (instancetype)init {
    self = [super initWithStyle:UITableViewStylePlain];
    if (self) {
        self.title = @"Orkid Tests";
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // Get available tests - do this in viewDidLoad to ensure proper initialization
    self.tests = [OrkTests availableTests];

    // Register cell class
    [self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:@"TestCell"];

    // Style
    self.tableView.backgroundColor = [UIColor colorWithRed:0.1f green:0.1f blue:0.1f alpha:1.0f];
    self.tableView.separatorColor = [UIColor colorWithRed:0.3f green:0.3f blue:0.3f alpha:1.0f];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return _tests ? _tests.count : 0;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"TestCell" forIndexPath:indexPath];

    OrkTestInfo* testInfo = _tests[indexPath.row];
    cell.textLabel.text = testInfo.name;
    cell.textLabel.textColor = [UIColor whiteColor];
    cell.backgroundColor = [UIColor colorWithRed:0.15f green:0.15f blue:0.15f alpha:1.0f];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

    return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    OrkTestInfo* testInfo = _tests[indexPath.row];

    // Create test detail view controller
    TestDetailViewController* detailVC = [[TestDetailViewController alloc] initWithTest:testInfo];
    [self.navigationController pushViewController:detailVC animated:YES];

    [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
    return 60.0f;
}

@end
