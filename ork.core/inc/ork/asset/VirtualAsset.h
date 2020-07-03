///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
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
