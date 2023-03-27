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
  void addChild(geditemnode_ptr_t w);
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

struct KeyDecoName {
  PropTypeString mActualKey;
  int miMultiIndex;

  PropTypeString DecoratedName() const;

  KeyDecoName(const char* pkey, int imulti); // decomposed name/index
  KeyDecoName(const char* pkey);             // precomposed name/index
};

///////////////////////////////////////////////////////////////////////////////

struct GedLabelNode : public GedItemNode {
public:
  ///////////////////////////////////////////////////

  GedLabelNode(ObjModel* mdl, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj);

private:
  virtual void DoDraw(lev2::Context* pTARG);
};
///////////////////////////////////////////////////////////////////////////////
struct GedMapNode : public GedItemNode {
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

  bool DoDrawDefault() const final {
    return false;
  }
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
