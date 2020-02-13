///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once
#if !defined(WII)
//#define ORKCONFIG_ASSET_UNLOAD
#endif
#include <ork/asset/AssetClass.h>
#include <ork/object/Object.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/PoolString.h>

#include <ork/config/config.h>

namespace ork { namespace asset {

class Asset : public Object
{
    RttiDeclareAbstractWithCategory( Asset, Object, AssetClass );

public:
  Asset();
  void SetName(PoolString name);
  PoolString GetName() const;
  virtual PoolString GetType() const;
  bool Load() const;
  bool LoadUnManaged() const;
  bool IsLoaded() const;

  varmap::VarMap _varmap;
  PoolString mName;
};

} }
