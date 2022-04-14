////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

namespace ork::reflect::serdes {
class BidirectionalSerializer;
template <typename T>
void Serialize(
    const T*, //
    T*,
    BidirectionalSerializer&);

template <typename T>
void Serialize(
    std::shared_ptr<const T>&, //
    std::shared_ptr<T>&,
    BidirectionalSerializer&);
} // namespace ork::reflect::serdes
