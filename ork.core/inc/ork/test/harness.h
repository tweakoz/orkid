////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/application/application.h>
#include <ork/kernel/svariant.h>

namespace ork { namespace test {

using appvar_t = svar128_t;

using test_setupfn_t = std::function<void(appvar_t& scoped_var)>;

int harness(
    appinitdata_ptr_t initdata,
    const char* test_exename,
    test_setupfn_t scoped_test_init_lambda);

}} // namespace ork::test
