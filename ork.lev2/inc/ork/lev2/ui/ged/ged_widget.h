#pragma once

#include "ged.h"
#include <ork/lev2/ui/surface.h>

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

struct GedWidget { //}: public ork::AutoConnector {
  //RttiDeclareAbstract(GedWidget, ork::AutoConnector);

  GedWidget(objectmodel_ptr_t model);
  ~GedWidget();

  void ComputeStackHash();

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

  GedSkin* GetSkin();
  void AddSkin(GedSkin* psk);
  void SetDims(int iw, int ih);

  static const int kdim = 8;

  geditemnode_ptr_t mRootItem;
  int miW;
  int miH;
  object_ptr_t mRootObject;
  objectmodel_ptr_t _model;
  std::deque<GedItemNode*> _itemstack;
  int miRootH;
  gedsurface_ptr_t _viewport;
  U64 mStackHash;
  orkvector<GedSkin*> mSkins;
  int miSkin;
  bool mbDeleteModel;
};

///////////////////////////////////////////////////////////////////////////////
class GedSurface : public ui::Surface {
public:
  // friend class lev2::PickBuffer<GedSurface>;

  GedSurface(const std::string& name, objectmodel_ptr_t model);
  ~GedSurface();

  fvec4 AssignPickId(GedObject* pobj);

  void ResetScroll();

  const GedObject* GetMouseOverNode() const;

  static orkset<GedSurface*> gAllViewports;
  void SetDims(int iw, int ih);
  void onInvalidate();

private:
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoSurfaceResize() override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  void _doGpuInit(lev2::Context* pt) final;

  objectmodel_ptr_t _model;
  GedWidget _widget;
  GedObject* mpActiveNode;
  int miScrollY;
  const GedObject* mpMouseOverNode;
  ork::msgrouter::subscriber_t _simulation_subscriber;
};
} //namespace ork::lev2::ged {
