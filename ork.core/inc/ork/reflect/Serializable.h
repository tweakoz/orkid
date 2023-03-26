////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/config/config.h>

namespace ork {

struct Object;

namespace reflect {

// This forces the pointer-to-member representation
// to always support multiple_inheritance of the class types.
#ifdef _MSVC
# pragma pointers_to_members( full_generality, multiple_inheritance )
#endif

// All reflection pointer-to-members are
// stored using Serializable as the class type,
// see of Section 5.2.9 Static cast, item 9,
// of the ISO C++ 98 standard.

// NOTE: DO NOT FORWARD DECLARE THIS CLASS
//  MSVC builds may not run correctly if properties are compiled 
//  with partial typeinfo on this class.
//class  Serializable { public: Serializable(); };

//typedef ork::Object Serializable;

} }

