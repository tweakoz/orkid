////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/kernel/ringbuffer.hpp>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/thread.h>
#include <memory>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

TEST(svariant_try)
{
    svar128_t var;

    var.set<bool>(true);

    CHECK_EQUAL(var.tryAs<float>(),false);
    CHECK_EQUAL(var.tryAs<bool>(),true);
    CHECK_EQUAL(var.tryAs<bool>().value(),true);

    var.set<float>(3.14f);
    CHECK_EQUAL(var.tryAs<float>(),true);
    CHECK_EQUAL(var.tryAs<bool>(),false);
    auto asf = var.tryAs<bool>();
    if( asf )
        printf("chk<%d> val<%d>\n", int(asf), int(asf.value()));

}

///////////////////////////////////////////////////////////////////////////////
// operator= and convertFromOtherSize must _destroy() the currently-held value
// before placement-copying the source in. Regression gate for the process-wide
// static_variant assignment leak: every opq Op held its lambda in a variant and
// Process() reassigned it via operator=, silently leaking each queued lambda's
// captured shared_ptrs.
///////////////////////////////////////////////////////////////////////////////

TEST(svariant_assign_destroys_held) {
  auto tracked = std::make_shared<int>(42);
  CHECK_EQUAL(tracked.use_count(), 1);

  {
    svar128_t held;
    held.set(tracked); // variant now holds a copy of the shared_ptr
    CHECK_EQUAL(tracked.use_count(), 2);

    svar128_t other;
    other.set<float>(3.14f);

    held = other; // must release the held shared_ptr before copying `other`
    CHECK_EQUAL(tracked.use_count(), 1);
    CHECK_EQUAL(held.tryAs<float>(), true);
  }
  CHECK_EQUAL(tracked.use_count(), 1);
}

///////////////////////////////////////////////////////////////////////////////
// self-assignment must be a no-op and must not destroy the held value.
///////////////////////////////////////////////////////////////////////////////

TEST(svariant_self_assign_safe) {
  auto tracked = std::make_shared<int>(7);
  svar128_t held;
  held.set(tracked);
  CHECK_EQUAL(tracked.use_count(), 2);

  svar128_t& alias = held;
  held             = alias; // self-assign: value must survive intact

  CHECK_EQUAL(tracked.use_count(), 2);
  CHECK_EQUAL(held.isA<std::shared_ptr<int>>(), true);
  CHECK_EQUAL(*held.get<std::shared_ptr<int>>(), 7);
}

///////////////////////////////////////////////////////////////////////////////
// Batch mirror of the opq scenario: N variants each holding a captured
// shared_ptr, all reassigned in place. Pre-fix, only end-of-run cases released;
// post-fix every reassignment releases exactly one hold.
///////////////////////////////////////////////////////////////////////////////

TEST(svariant_batch_reassign_no_leak) {
  static constexpr int N = 38;
  auto token             = std::make_shared<int>(0);

  std::vector<svar128_t> slots(N);
  for (int i = 0; i < N; i++)
    slots[i].set(token);
  CHECK_EQUAL(token.use_count(), 1 + N);

  // reassign every slot to a plain value; each must release its token hold
  for (int i = 0; i < N; i++) {
    svar128_t repl;
    repl.set<int>(i);
    slots[i] = repl;
  }
  CHECK_EQUAL(token.use_count(), 1);
}
