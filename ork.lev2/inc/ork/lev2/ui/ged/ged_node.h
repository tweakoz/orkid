#pragma once

#include "ged.h"
#include <ork/rtti/RTTIX.inl>

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

struct GedSkin;

struct GedObject : public ork::Object {

  DeclareAbstractX(GedObject, ork::Object);

  int miD;
  int miDecoIndex;

  GedObject()
      : miD(0)
      , miDecoIndex(0) {
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
  DeclareAbstractX(GedItemNode, GedObject);

public:
  ///////////////////////////////////////////////////

  GedItemNode(GedContainer* container,
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
  void addChild(geditemnode_ptr_t w);
  int numChildren() const;
  geditemnode_ptr_t child(int idx) const;
  ///////////////////////////////////////////////////
  virtual int CalcHeight();
  /*geditemnode_ptr_t GetChildContainer() {
    return this;
  }*/
  ///////////////////////////////////////////////////
  bool IsObjectHilighted(const GedObject* pobj) const;
  ///////////////////////////////////////////////////
  virtual void DoDraw(lev2::Context* pTARG) = 0;
  ///////////////////////////////////////////////////
  using NameType = std::string;

  //
  GedContainer* _container = nullptr;
  bool mbcollapsed = false;
  std::string _propname;
  const reflect::ObjectProperty* _property;
  object_ptr_t _object;
  //
  std::string mvalue;
  orkvector<geditemnode_ptr_t> _children;
  orkmap<std::string, std::string> mTags;
  std::string _content;
  bool mbVisible = true;
  bool mbInvalid = true;
  bool _doDrawDefault = true;
  ObjModel* _model = nullptr;
  GedItemNode* _parent = nullptr;
  int miX = 0;
  int miY = 0;
  int miW = 0;
  int miH = 0;
  int micalch = 0;
  ///////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
class GedRootNode : public GedItemNode {

  DeclareAbstractX(GedRootNode, GedItemNode);

  void DoDraw(lev2::Context* pTARG) final;
  void Layout(int ix, int iy, int iw, int ih) final;
  int CalcHeight() final;

public:
  GedRootNode(GedContainer* c, //
              const char* name,  //
              const reflect::ObjectProperty* prop,  //
              object_ptr_t obj);

  bool DoDrawDefault() const {
    return false;
  } // virtual
};
///////////////////////////////////////////////////////////////////////////////
struct GedGroupNode : public GedItemNode {

  DeclareAbstractX(GedGroupNode, GedItemNode);

  void DoDraw(lev2::Context* pTARG) final;
  bool mbCollapsed;
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;
  ork::file::Path::NameType mPersistID;

  GedGroupNode(GedContainer* container, //
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

struct KeyDecoName {
  PropTypeString mActualKey;
  int miMultiIndex;

  PropTypeString DecoratedName() const;

  KeyDecoName(const char* pkey, int imulti); // decomposed name/index
  KeyDecoName(const char* pkey);             // precomposed name/index
};

///////////////////////////////////////////////////////////////////////////////

struct GedLabelNode : public GedItemNode {
  DeclareAbstractX(GedLabelNode, GedItemNode);
public:
  ///////////////////////////////////////////////////

  GedLabelNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj);

private:
  void DoDraw(lev2::Context* pTARG) final;
};
///////////////////////////////////////////////////////////////////////////////
struct GedMapNode : public GedItemNode {

  DeclareAbstractX(GedMapNode, GedItemNode);

public:

  using event_constptr_t = ork::ui::event_constptr_t;

  GedMapNode(ObjModel* mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);
  const orkmap<PropTypeString, KeyDecoName>& GetKeys() const {
    return mMapKeys;
  }
  void FocusItem(const PropTypeString& key);

  bool IsKeyPresent(const KeyDecoName& pkey) const;
  void AddKey(const KeyDecoName& pkey);

  bool IsMultiMap() const {
    return mbIsMultiMap;
  }

private:
  const reflect::IMap* mMapProp;
  orkmap<PropTypeString, KeyDecoName> mMapKeys;
  GedItemNode* mKeyNode;
  PropTypeString mCurrentKey;
  int mItemIndex;
  bool mbSingle;
  bool mbConst;
  bool mbIsMultiMap;
  bool mbImpExp;

  void OnMouseDoubleClicked(event_constptr_t ev) final;

  void CheckVis();
  void DoDraw(Context* pTARG) final;

  void AddItem(event_constptr_t ev);
  void RemoveItem(event_constptr_t ev);
  void MoveItem(event_constptr_t ev);
  void DuplicateItem(event_constptr_t ev);
  void ImportItem(event_constptr_t ev);
  void ExportItem(event_constptr_t ev);
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
