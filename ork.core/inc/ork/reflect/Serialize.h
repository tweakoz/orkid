////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
