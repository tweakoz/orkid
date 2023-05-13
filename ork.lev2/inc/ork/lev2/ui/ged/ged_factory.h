#pragma once

#include "ged.h"

namespace ork::lev2::ged {
struct GedNodeFactory : public ::ork::Object {
  virtual geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const  = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryEnum : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryEnum, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryTransform : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryTransform, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactory_PlugFloat : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactory_PlugFloat, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;
  //void Recurse(GedContainer* c, const reflect::ObjectProperty* prop, ork::Object* pobj) const final;

public:
  GedNodeFactory_PlugFloat();
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryOutliner : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryOutliner, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryGradient : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryGradient, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryCurve1D : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryCurve1D, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryAssetList : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryAssetList, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryFileList : public GedNodeFactory {
  RttiDeclareConcrete(GedNodeFactoryFileList, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
