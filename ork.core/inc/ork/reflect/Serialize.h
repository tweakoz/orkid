////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

namespace ork { namespace reflect {
class BidirectionalSerializer;

template<typename T>
void Serialize(const T *, T *, BidirectionalSerializer &);

} }

