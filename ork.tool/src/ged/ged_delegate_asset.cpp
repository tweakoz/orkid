////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include "ged_delegate_asset.hpp"
#include <orktool/ged/ged_io.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/asset/Asset.h>
#include <ork/reflect/serialize/LayerSerializer.h>
#include <ork/reflect/serialize/LayerDeserializer.h>
#include <ork/reflect/serialize/NullSerializer.h>
#include <ork/reflect/serialize/NullDeserializer.h>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryAssetList, "ged.factory.assetlist");

namespace ork { namespace tool { namespace ged {

class GedAssetObjIoDriver : public IoDriverBase {
public:
  GedAssetObjIoDriver(ObjModel& Model, const reflect::ObjectProperty* prop, Object* obj)
      : IoDriverBase(Model, prop, obj) {
  }
  void SetValue(ork::Object* passet);
  void GetValue(const ork::Object*& rp) const;
};

///////////////////////////////////////////////////////////////////////////////

class AssLayerSerializer : public reflect::serdes::LayerSerializer {
  const GedAssetObjIoDriver& mIoDriver;
  const ork::asset::Asset* mpAsset;
  reflect::serdes::NullSerializer mNullSer;
  /////////////////////////////////////////////////////
  bool Serialize(const rtti::ICastable* value) /*virtual*/
  {
    mpAsset = rtti::autocast(value);
    return reflect::serdes::LayerSerializer::Serialize(value);
  }
  /////////////////////////////////////////////////////
public:
  /////////////////////////////////////////////////////
  AssLayerSerializer(const GedAssetObjIoDriver& iodriver)
      : reflect::serdes::LayerSerializer(mNullSer)
      , mpAsset(0)
      , mIoDriver(iodriver) {
  }
  /////////////////////////////////////////////////////
  const ork::asset::Asset* asset() {
    reflect::serdes::LayerSerializer::serializeObjectProperty(
        mIoDriver.GetProp(), //
        mIoDriver.GetObject());
    return mpAsset;
  }
  /////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class AssLayerDeserializer : public reflect::serdes::LayerDeserializer {
  ork::asset::Asset* mpAsset;
  reflect::serdes::NullDeserializer mNullDeser;
  GedAssetObjIoDriver& mIoDriver;

  /////////////////////////////////////////////////////
  bool Deserialize(rtti::ICastable*& value) /*virtual*/
  {
    value = mpAsset;
    return true;
  }
  /////////////////////////////////////////////////////
public:
  /////////////////////////////////////////////////////
  AssLayerDeserializer(GedAssetObjIoDriver& iodriver)
      : reflect::serdes::LayerDeserializer(mNullDeser)
      , mpAsset(0)
      , mIoDriver(iodriver) {
  }
  /////////////////////////////////////////////////////
  void SetAsset(ork::asset::Asset* passet) {
    // mIoDriver.GetModel().SigPreNewObject();
    mpAsset = passet;
    reflect::serdes::LayerDeserializer::deserializeObjectProperty(
        mIoDriver.GetProp(), //
        mIoDriver.GetObject());
    mIoDriver.GetModel().SigModelInvalidated();
  }
  /////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

void GedAssetObjIoDriver::SetValue(ork::Object* passet) {
  AssLayerDeserializer deser(*this);
  ork::asset::Asset* pas = rtti::autocast(passet);
  deser.SetAsset(pas);
  ObjectGedEditEvent ev;
  ev.mProperty = GetProp();
  GetObject()->Notify(&ev);
}

///////////////////////////////////////////////////////////////////////////////

void GedAssetObjIoDriver::GetValue(const ork::Object*& rp) const {
  AssLayerSerializer ser(*this);
  rp = ser.asset();
}

///////////////////////////////////////////////////////////////////////////////

void GedFactoryAssetList::Describe() {
}

GedItemNode*
GedFactoryAssetList::CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj)
    const {
  GedAssetNode<GedAssetObjIoDriver>* itemnode = new GedAssetNode<GedAssetObjIoDriver>(mdl, Name.c_str(), prop, obj);
  itemnode->SetLabel();
  return itemnode;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}}} // namespace ork::tool::ged

typedef ork::tool::ged::GedAssetNode<ork::tool::ged::GedAssetObjIoDriver> GedAssetNodeObj;
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GedAssetNodeObj, "GedAssetNodeObj");
template <> void ork::tool::ged::GedAssetNode<ork::tool::ged::GedAssetObjIoDriver>::Describe() {
}
template class ork::tool::ged::GedAssetNode<ork::tool::ged::GedAssetObjIoDriver>;
