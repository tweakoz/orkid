////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
