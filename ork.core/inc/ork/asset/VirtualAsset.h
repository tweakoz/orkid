////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>

namespace ork { namespace asset {

class VirtualAsset : public Asset {
  RttiDeclareConcrete(VirtualAsset, Asset);

public:
  VirtualAsset();

  void setType(std::string category);

  std::string type() const final;

private:
  std::string _category;
};

}} // namespace ork::asset
