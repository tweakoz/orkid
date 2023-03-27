#pragma once

#include "ged.h"

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////
class GedFactoryEnum : public GedFactory {
  RttiDeclareConcrete(GedFactoryEnum, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryTransform : public GedFactory {
  RttiDeclareConcrete(GedFactoryTransform, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactory_PlugFloat : public GedFactory {
  RttiDeclareConcrete(GedFactory_PlugFloat, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;
  void Recurse(ObjModel& mdl, const reflect::ObjectProperty* prop, ork::Object* pobj) const final;

public:
  GedFactory_PlugFloat();
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryOutliner : public GedFactory {
  RttiDeclareConcrete(GedFactoryOutliner, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryGradient : public GedFactory {
  RttiDeclareConcrete(GedFactoryGradient, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryCurve : public GedFactory {
  RttiDeclareConcrete(GedFactoryCurve, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryAssetList : public GedFactory {
  RttiDeclareConcrete(GedFactoryAssetList, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryFileList : public GedFactory {
  RttiDeclareConcrete(GedFactoryFileList, GedFactory);
  GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const final;

public:
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
