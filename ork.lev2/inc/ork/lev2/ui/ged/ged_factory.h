#pragma once

#include "ged.h"

namespace ork::lev2::ged {
struct GedNodeFactory : public ::ork::Object {
  DeclareAbstractX(GedNodeFactory, ::ork::Object);
  virtual geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const  = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryEnum : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryEnum, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryTransform : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryTransform, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryPlug : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryPlug, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;
public:
  GedNodeFactoryPlug();
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryColorV4 : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryColorV4, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;
public:
  GedNodeFactoryColorV4();
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryPlugFloatXF : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryPlugFloatXF, GedNodeFactory);
  geditemnode_ptr_t //
  createItemNode(GedContainer* c, const ConstString& name, newiodriver_ptr_t iodriver ) const final;
  GedNodeFactoryPlugFloatXF();
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryOutliner : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryOutliner, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryGradient : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryGradient, GedNodeFactory);
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
  DeclareConcreteX(GedNodeFactoryAssetList, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};
///////////////////////////////////////////////////////////////////////////////
struct GedNodeFactoryFileList : public GedNodeFactory {
  DeclareConcreteX(GedNodeFactoryFileList, GedNodeFactory);
  geditemnode_ptr_t
  createItemNode(GedContainer* c, const ConstString& Name, newiodriver_ptr_t iodriver ) const final;

public:
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
