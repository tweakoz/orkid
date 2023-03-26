////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
