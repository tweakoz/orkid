///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2009, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/Array.h>
#include <ork/kernel/any.h>
#include <ork/kernel/gstack.h>
#include <ork/math/collision_test.h>
#include <ork/math/sphere.h>

namespace ork {

class SphereTreeNode {
public:
  any32 mAny;
  Sphere mSphere;
  SphereTreeNode* mChildren[2];

  SphereTreeNode() : mSphere(fvec3::Zero(), 0.0f) {
    mChildren[0] = 0;
    mChildren[1] = 0;
  }

  const any32& UserData() const { return mAny; }
  any32& UserData() { return mAny; }
};

class SphereTree {
public:
  mutable fixed_stack<const SphereTreeNode*, 128> mNodeStack;
  SphereTreeNode mRoot;
  SphereTreeNode& GetRoot() { return mRoot; }

  inline void RayTest(const fray3& ray, Ray3HitTest& test_set) const;
};

///////////////////////////////////////////////

inline void SphereTree::RayTest(const fray3& ray, Ray3HitTest& test_set) const {
  static int inumhit = 0;
  static int inummiss = 0;

  mNodeStack.push(&mRoot);

  while (mNodeStack.size()) {
    ++test_set.miSphTests;

    const SphereTreeNode* pnode = mNodeStack.top();
    mNodeStack.pop();

    float ft = 0.0f;
    bool bcol = CollisionTester::RaySphereTest(ray, pnode->mSphere, ft);

    if (bcol) {
      ++inumhit;
      ++test_set.miSphTestsPassed;

      int inumc = 0;
      if (pnode->mChildren[0]) {
        mNodeStack.push(pnode->mChildren[0]);
        ++inumc;
      }
      if (pnode->mChildren[1]) {
        mNodeStack.push(pnode->mChildren[1]);
        ++inumc;
      }
      if (0 == inumc) {
        test_set.OnHit(pnode->UserData(), ray);
      }
    } else {
      ++inummiss;
    }
  }
}

///////////////////////////////////////////////

} // namespace ork
