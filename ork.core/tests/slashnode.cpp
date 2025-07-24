////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/kernel/slashnode.h>

TEST(SlashNodeWalkToRoot) {

  auto slashtree = std::make_shared<ork::SlashTree>();
  auto node_3    = slashtree->addNode("/one/two/three", nullptr);
  auto node_2    = node_3->_parent.lock();
  auto node_1    = node_2->_parent.lock();
  auto node_r    = node_1->_parent.lock();
  CHECK_EQUAL(node_3->pathAsString(), "/one/two/three");
  CHECK_EQUAL(node_2->pathAsString(), "/one/two");
  CHECK_EQUAL(node_1->pathAsString(), "/one");
  CHECK_EQUAL(node_r->pathAsString(), "/");

  CHECK_EQUAL(node_r->_parent.lock(), ork::slashnode_ptr_t());

  auto node_4 = slashtree->addNode("/one/two/four", (void*)0x1234);

  CHECK_EQUAL(node_3->_parent.lock(), node_4->_parent.lock());
  CHECK_EQUAL(node_4->pathAsString(), "/one/two/four");
  CHECK_EQUAL(node_4->_data, (void*)0x1234);

  // printf("n3: %s\n", node_3->pathAsString().c_str());
  // printf("n2: %s\n", node_2->pathAsString().c_str());
  // printf("n1: %s\n", node_1->pathAsString().c_str());
  // printf("nr: %s\n", node_r->pathAsString().c_str());
}

TEST(SlashNodeBreakupPath) {
  orkvector<std::string> parts;
  
  // Test simple path
  ork::breakup_slash_path("/one/two/three", parts);
  CHECK_EQUAL(3, parts.size());
  CHECK_EQUAL("one", parts[0]);
  CHECK_EQUAL("two", parts[1]);
  CHECK_EQUAL("three", parts[2]);
  
  // Test path with file and extension
  parts.clear();
  ork::breakup_slash_path("/path/to/file.txt", parts);
  CHECK_EQUAL(3, parts.size());
  CHECK_EQUAL("path", parts[0]);
  CHECK_EQUAL("to", parts[1]);
  CHECK_EQUAL("file.txt", parts[2]);
  
  // Test path with URL base
  parts.clear();
  ork::breakup_slash_path("data://assets/texture.png", parts);
  CHECK_EQUAL(3, parts.size());
  CHECK_EQUAL("data://", parts[0]);
  CHECK_EQUAL("assets", parts[1]);
  CHECK_EQUAL("texture.png", parts[2]);
  
  // Test empty path segments
  parts.clear();
  ork::breakup_slash_path("//double//slash//", parts);
  CHECK_EQUAL(2, parts.size());
  CHECK_EQUAL("double", parts[0]);
  CHECK_EQUAL("slash", parts[1]);
}

TEST(SlashNodeRemoval) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  
  // Add some nodes
  auto node_a = slashtree->addNode("/branch/a", (void*)0xa);
  auto node_b = slashtree->addNode("/branch/b", (void*)0xb);
  auto node_c = slashtree->addNode("/branch/c/deep", (void*)0xc);
  
  // Check they exist
  CHECK_EQUAL(3, node_a->_parent.lock()->numChildren()); // a, b, and c
  CHECK_EQUAL(node_a->_parent.lock(), node_b->_parent.lock());
  
  // Remember the parent
  auto branch = node_a->_parent.lock();
  
  // Remove a node
  slashtree->removeNode(node_a.get());
  
  // Check it was removed
  CHECK_EQUAL(2, branch->numChildren()); // still has b and c
  
  // Remove node_b
  slashtree->removeNode(node_b.get());
  CHECK_EQUAL(1, branch->numChildren()); // only c remains
}

TEST(SlashNodeChildrenAndLeaves) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  
  auto leaf = slashtree->addNode("/parent/child/leaf", nullptr);
  auto child = leaf->_parent.lock();
  auto parent = child->_parent.lock();
  
  CHECK_EQUAL(true, leaf->isLeaf());
  CHECK_EQUAL(false, child->isLeaf());
  CHECK_EQUAL(false, parent->isLeaf());
  
  CHECK_EQUAL(1, parent->numChildren());
  CHECK_EQUAL(1, child->numChildren());
  CHECK_EQUAL(0, leaf->numChildren());
  
  CHECK_EQUAL("leaf", leaf->nodeName());
  CHECK_EQUAL("child", child->nodeName());
  CHECK_EQUAL("parent", parent->nodeName());
}

TEST(SlashNodeGetPath) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  auto node = slashtree->addNode("/a/b/c/d", nullptr);
  
  orkvector<const ork::SlashNode*> path;
  node->getPath(path);
  
  CHECK_EQUAL(5, path.size()); // root + 4 segments
  CHECK_EQUAL("", path[0]->nodeName()); // root has empty name
  CHECK_EQUAL("a", path[1]->nodeName());
  CHECK_EQUAL("b", path[2]->nodeName());
  CHECK_EQUAL("c", path[3]->nodeName());
  CHECK_EQUAL("d", path[4]->nodeName());
}

TEST(SlashNodeStrCueToChar) {
  // Test finding character
  auto pos = ork::str_cue_to_char("hello/world", '/', 0);
  CHECK_EQUAL(5, pos);
  
  // Test starting from middle
  pos = ork::str_cue_to_char("hello/world/test", '/', 6);
  CHECK_EQUAL(11, pos);
  
  // Test not found
  pos = ork::str_cue_to_char("hello world", '/', 0);
  CHECK_EQUAL(std::string::npos, pos);
  
  // Test empty string
  pos = ork::str_cue_to_char("", '/', 0);
  CHECK_EQUAL(std::string::npos, pos);
}

TEST(SlashNodeClear) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  
  // Add nodes
  slashtree->addNode("/one/two", nullptr);
  slashtree->addNode("/three/four", nullptr);
  
  // Verify root has children
  CHECK_EQUAL(2, slashtree->_root->numChildren());
  
  // Clear and verify
  slashtree->clear();
  CHECK_EQUAL(0, slashtree->_root->numChildren());
  CHECK_EQUAL("", slashtree->_root->nodeName());
}

TEST(SlashNodeDataStorage) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  
  int data1 = 42;
  float data2 = 3.14f;
  std::string data3 = "test";
  
  auto node1 = slashtree->addNode("/data/int", &data1);
  auto node2 = slashtree->addNode("/data/float", &data2);
  auto node3 = slashtree->addNode("/data/string", &data3);
  
  CHECK_EQUAL(&data1, node1->data());
  CHECK_EQUAL(&data2, node2->data());
  CHECK_EQUAL(&data3, node3->data());
  
  // Test setData
  int newdata = 99;
  node1->setData(&newdata);
  CHECK_EQUAL(&newdata, node1->data());
}

TEST(SlashNodeRootAccess) {
  auto slashtree = std::make_shared<ork::SlashTree>();
  auto deep_node = slashtree->addNode("/very/deep/nested/path", nullptr);
  
  auto root = deep_node->root();
  CHECK_EQUAL(slashtree->_root.get(), root);
  CHECK_EQUAL(slashtree->root().get(), root);
  
  // Root's root is itself
  CHECK_EQUAL(root, root->root());
}
