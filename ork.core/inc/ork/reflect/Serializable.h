////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/config/config.h>

namespace ork {

class Object;

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

