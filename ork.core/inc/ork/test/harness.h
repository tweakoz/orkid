////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/kernel/svariant.h>

namespace ork { namespace test {

using test_setupfn_t = std::function<void(svar16_t& scoped_var)>;

int harness(
    int argc, //
    char** argv,
    char** envp,
    const char* test_exename,
    test_setupfn_t scoped_test_init_lambda);

}} // namespace ork::test
