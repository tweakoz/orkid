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
#include <ork/kernel/dvariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/thread.h>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

TEST(dvariant_try)
{
    dvar_t var;

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
