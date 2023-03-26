////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <limits>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::Float {

inline float Pi() { return float(3.1415927f); }
inline float E() { return float(2.7182818f); }
inline float Epsilon() { return float(0.00001f); }
inline float FloatEpsilon() { return std::numeric_limits<float>::epsilon(); }
inline double DoubleEpsilon() { return std::numeric_limits<double>::epsilon(); }

} // namespace ork
