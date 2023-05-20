#pragma once

#include <string>
#include <functional>
#include <ork/orktypes.h>

namespace ork::reflect {

using string_t = std::string;
using obj_to_string_fn_t = std::function<string_t(object_ptr_t)>;

}