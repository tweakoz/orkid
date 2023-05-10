#pragma once

#include "ged.h"

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

struct GedContainer { //}: public ork::AutoConnector {
  //RttiDeclareAbstract(GedContainer, ork::AutoConnector);

  static gedcontainer_ptr_t createShared(objectmodel_ptr_t mdl);

  GedContainer(objectmodel_ptr_t model);
  ~GedContainer();

  void ComputeStackHash();
  void gpuInit(lev2::Context* context);

  //////////////////////////////////////////////////////////////

  //DeclarePublicSignal(Repaint);
  //DeclarePublicAutoSlot(Repaint);
  //DeclarePublicAutoSlot(ModelInvalidated);

  void SlotRepaint();
  void SlotModelInvalidated();

  //////////////////////////////////////////////////////////////

  void SetDeleteModel(bool bv) {
    mbDeleteModel = true;
  }

  void PropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop);

  void Attach(ork::Object* obj);

  void Draw(lev2::Context* pTARG, int iw, int ih, int iscrolly);

  geditemnode_ptr_t GetRootItem() const {
    return mRootItem;
  }

  void IncrementSkin();
  GedItemNode* ParentItemNode() const;
  void PushItemNode(GedItemNode* qw);
  void PopItemNode(GedItemNode* qw);
  void AddChild(geditemnode_ptr_t pw);
  void DoResize();
  void OnSelectionChanged();
  int GetStackDepth() const {
    return int(_itemstack.size());
  }
  int GetRootHeight() const {
    return miRootH;
  }

  U64 GetStackHash() const;

  void SetDims(int iw, int ih);
  static const int kdim = 8;

  geditemnode_ptr_t mRootItem;
  int miW;
  int miH;
  object_ptr_t mRootObject;
  objectmodel_ptr_t _model;
  std::deque<GedItemNode*> _itemstack;
  int miRootH;
  GedSurface* _viewport;
  U64 mStackHash;
  orkvector<GedSkin*> mSkins;
  GedSkin* _activeSkin = nullptr;

  int _skin_index;
  bool mbDeleteModel;
  sigslot2::scoped_connection _connection_modelinvalidated;

};


} //namespace ork::lev2::ged {
