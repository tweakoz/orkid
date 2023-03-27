#pragma once

#include "ged.h"

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

struct GedSkin;

struct GedObject { //} : public ork::Object {

  int miD;
  int miDecoIndex;

  GedObject()
      : miD(0)
      , miDecoIndex(0) {
  }
  virtual ~GedObject() {
  }

  void SetDepth(int id) {
    miD = id;
  }
  int GetDepth() const {
    return miD;
  }
  int GetDecoIndex() const {
    return miDecoIndex;
  }
  void SetDecoIndex(int idx) {
    miDecoIndex = idx;
  }

  virtual void OnMouseDragged(ork::ui::event_constptr_t ev) {
  }
  virtual void OnMouseMoved(ork::ui::event_constptr_t ev) {
  }
  virtual void OnMouseClicked(ork::ui::event_constptr_t ev) {
  }
  virtual void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
  }
  virtual void OnMouseReleased(ork::ui::event_constptr_t ev) {
  }

  virtual void OnUiEvent(ork::ui::event_constptr_t ev);
};

///////////////////////////////////////////////////////////////////////////////

struct GedItemNode : public GedObject {
  //RttiDeclareAbstract(GedItemNode, GedObject);

public:
  ///////////////////////////////////////////////////

  GedItemNode(ObjModel* mdl, //
              const char* name,  //
              const reflect::ObjectProperty* prop,  //
              object_ptr_t obj);

  ///////////////////////////////////////////////////

  ~GedItemNode() override;

  void SetVisible(bool bv) {
    mbVisible = bv;
  }
  bool IsVisible() const {
    return mbVisible;
  }

  void SetXY(int ix, int iy) {
    miX = ix;
    miY = iy;
  }
  void SetWH(int iw, int ih) {
    miW = iw;
    miH = ih;
  }
  int GetX() const {
    return miX;
  }
  int GetY() const {
    return miY;
  }

  void DestroyChildren();

  ///////////////////////////////////////////////////

  /*void activate() {
    onActivate();
  }
  void deactivate() {
    onDeactivate();
  }*/
  ///////////////////////////////////////////////////

  void SigInvalidateProperty();

  void Init();

  int height() const {
    return micalch;
  }
  int width() const {
    return miW;
  }

  virtual void Layout(int ix, int iy, int iw, int ih);
  virtual bool CanSideBySide() const {
    return false;
  }
  virtual bool DoDrawDefault() const; // { return true; }
  virtual void Invalidate() {
    mbInvalid = true;
  }
  virtual void ReSync() {
  }
  ///////////////////////////////////////////////////
  void Draw(lev2::Context* pTARG);
  ///////////////////////////////////////////////////
  virtual void onActiGedObjectvate() {
  }
  virtual void onDeactivate() {
  }
  ///////////////////////////////////////////////////
  int get_charh() const;
  int get_charw() const;
  int get_text_center_y() const;
  ///////////////////////////////////////////////////
  int contentWidth() const;
  int propnameWidth() const;
  int contentCenterX() const {
    return miX + (miW >> 1) - (contentWidth() >> 1);
  }
  int propnameCenterX() const {
    return miX + (miW >> 1) - (propnameWidth() >> 1);
  }
  ///////////////////////////////////////////////////
  void AddItem(geditemnode_ptr_t w);
  int numChildren() const;
  ///////////////////////////////////////////////////
  virtual int CalcHeight(void);
  /*geditemnode_ptr_t GetChildContainer() {
    return this;
  }*/
  ///////////////////////////////////////////////////
  bool IsObjectHilighted(const GedObject* pobj) const;
  ///////////////////////////////////////////////////
  GedSkin* activeSkin() const;
  virtual void DoDraw(lev2::Context* pTARG) = 0;
  ///////////////////////////////////////////////////
  using NameType = std::string;

  ObjModel* _model = nullptr;
  const reflect::ObjectProperty* _property;
  object_ptr_t _object;

  static GedSkin* gpSkin0;
  static GedSkin* gpSkin1;
  static int giSkin;
  int miX, miY;
  int miW, miH;
  bool mbVisible;
  bool mbInvalid;


  bool mbcollapsed;
  std::string mvalue;
  orkvector<geditemnode_ptr_t> _children;
  GedItemNode* _parent = nullptr;
  orkmap<std::string, std::string> mTags;
  gedwidget_ptr_t _widget;
  int micalch;
  ///////////////////////////////////////////////////
  std::string _propname;
  std::string _content;
  ///////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
class GedRootNode : public GedItemNode {
  virtual void DoDraw(lev2::Context* pTARG);
  virtual void Layout(int ix, int iy, int iw, int ih);
  virtual int CalcHeight(void);

public:
  GedRootNode(ObjModel* mdl, //
              const char* name,  //
              const reflect::ObjectProperty* prop,  //
              object_ptr_t obj);

  bool DoDrawDefault() const {
    return false;
  } // virtual
};
///////////////////////////////////////////////////////////////////////////////
struct GedGroupNode : public GedItemNode {
  virtual void DoDraw(lev2::Context* pTARG);
  bool mbCollapsed;
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;
  ork::file::Path::NameType mPersistID;

  GedGroupNode(ObjModel* mdl, //
               const char* name, //
               const reflect::ObjectProperty* prop, //
               object_ptr_t obj, //
               bool is_obj_node = false);

  void CheckVis();
  bool DoDrawDefault() const {
    return false;
  } // virtual
  bool mIsObjNode;
};

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
} //namespace ork::lev2::ged {
