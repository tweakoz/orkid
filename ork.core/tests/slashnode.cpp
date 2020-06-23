#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/kernel/slashnode.h>

TEST(SlashNodeWalkToRoot) {

  auto slashtree = std::make_shared<ork::SlashTree>();
  auto node_3    = slashtree->add_node("/one/two/three", nullptr);
  auto node_2    = node_3->_parent;
  auto node_1    = node_2->_parent;
  auto node_r    = node_1->_parent;
  CHECK_EQUAL(node_3->pathAsString(), "/one/two/three");
  CHECK_EQUAL(node_2->pathAsString(), "/one/two");
  CHECK_EQUAL(node_1->pathAsString(), "/one");
  CHECK_EQUAL(node_r->pathAsString(), "/");

  CHECK_EQUAL(node_r->_parent, (const ork::SlashNode*)nullptr);

  auto node_4 = slashtree->add_node("/one/two/four", (void*)0x1234);

  CHECK_EQUAL(node_3->_parent, node_4->_parent);
  CHECK_EQUAL(node_4->pathAsString(), "/one/two/four");
  CHECK_EQUAL(node_4->_data, (void*)0x1234);

  // printf("n3: %s\n", node_3->pathAsString().c_str());
  // printf("n2: %s\n", node_2->pathAsString().c_str());
  // printf("n1: %s\n", node_1->pathAsString().c_str());
  // printf("nr: %s\n", node_r->pathAsString().c_str());
}
