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

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

TEST(svariant_try)
{
    svar128_t var;

    var.Set<bool>(true);

    CHECK_EQUAL(var.TryAs<float>(),false);
    CHECK_EQUAL(var.TryAs<bool>(),true);
    CHECK_EQUAL(var.TryAs<bool>().value(),true);

    var.Set<float>(3.14f);
    CHECK_EQUAL(var.TryAs<float>(),true);
    CHECK_EQUAL(var.TryAs<bool>(),false);
    auto asf = var.TryAs<bool>();
    if( asf )
        printf("chk<%d> val<%d>\n", int(asf), int(asf.value()));

}
