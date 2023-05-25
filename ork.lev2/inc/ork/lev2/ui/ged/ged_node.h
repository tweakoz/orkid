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
  std::string _propname;

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

  virtual bool OnMouseDragged(ork::ui::event_constptr_t ev) {
    return true;
  }
  virtual bool OnMouseMoved(ork::ui::event_constptr_t ev) {
    return true;
  }
  virtual bool OnMouseClicked(ork::ui::event_constptr_t ev) {
    return true;
  }
  virtual bool OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
    return true;
  }
  virtual bool OnMouseReleased(ork::ui::event_constptr_t ev) {
    return true;
  }

  virtual bool OnUiEvent(ork::ui::event_constptr_t ev);
};

using gedobject_ptr_t = std::shared_ptr<GedObject>;

///////////////////////////////////////////////////////////////////////////////

struct GedItemNode : public GedObject {
  DeclareAbstractX(GedItemNode, GedObject);

public:
  ///////////////////////////////////////////////////

  GedItemNode(
      GedContainer* container,
      const char* name,                    //
      const reflect::ObjectProperty* prop, //
      object_ptr_t obj);

  GedItemNode(
      GedContainer* container,
      const char* name,                    //
      newiodriver_ptr_t iodriver );

  ///////////////////////////////////////////////////

  ~GedItemNode() override;

  void SetVisible(bool bv);
  bool IsVisible() const;
  void SetXY(int ix, int iy);
  void SetWH(int iw, int ih);
  int GetX() const;
  int GetY() const;

  ///////////////////////////////////////////////////

  void SigInvalidateProperty();

  void Init();

  int height() const;
  int width() const;

  virtual void Layout(int ix, int iy, int iw, int ih);
  virtual bool CanSideBySide() const;
  virtual void Invalidate();
  virtual void ReSync();
  ///////////////////////////////////////////////////
  void Draw(lev2::Context* pTARG);
  ///////////////////////////////////////////////////
  virtual void onActiGedObjectvate();
  virtual void onDeactivate();
  ///////////////////////////////////////////////////
  int get_charh() const;
  int get_charw() const;
  int get_text_center_y() const;
  ///////////////////////////////////////////////////
  int contentWidth() const;
  int propnameWidth() const;
  int contentCenterX() const;
  int propnameCenterX() const;
  ///////////////////////////////////////////////////
  void addChild(geditemnode_ptr_t w);
  int numChildren() const;
  geditemnode_ptr_t child(int idx) const;
  ///////////////////////////////////////////////////
  int computeHeight() const;
  virtual int doComputeHeight() const;
  ///////////////////////////////////////////////////
  bool IsObjectHilighted(const GedObject* pobj) const;
  ///////////////////////////////////////////////////
  virtual void DoDraw(lev2::Context* pTARG) = 0;
  ///////////////////////////////////////////////////
  lev2::Context* _l2context() const;
  ///////////////////////////////////////////////////
  using NameType = std::string;

  //
  GedContainer* _container = nullptr;
  bool mbcollapsed         = false;
  const reflect::ObjectProperty* _property;
  object_ptr_t _object;
  //
  std::string mvalue;
  orkvector<geditemnode_ptr_t> _children;
  orkmap<std::string, std::string> mTags;
  std::string _content;
  bool mbVisible       = true;
  bool mbInvalid       = true;
  bool _doDrawDefault  = true;
  GedItemNode* _parent = nullptr;
  int miX              = 0;
  int miY              = 0;
  int miW              = 0;
  int miH              = 0;
  mutable int micalch          = 0;

  newiodriver_ptr_t _iodriver;
  svar64_t _impl;

  ///////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
class GedRootNode : public GedItemNode {

  DeclareAbstractX(GedRootNode, GedItemNode);

  void DoDraw(lev2::Context* pTARG) final;
  void Layout(int ix, int iy, int iw, int ih) final;

public:
  GedRootNode(
      GedContainer* c,                     //
      const char* name,                    //
      const reflect::ObjectProperty* prop, //
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
  bool OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;
  ork::file::Path::NameType mPersistID;

  GedGroupNode(
      GedContainer* container,             //
      const char* name,                    //
      const reflect::ObjectProperty* prop, //
      object_ptr_t obj,                    //
      bool is_obj_node = false);

  void updateVisibility();
  int doComputeHeight() const final;
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

  GedMapNode(GedContainer* c, const char* name, const reflect::IMap* prop, object_ptr_t obj);
  const orkmap<PropTypeString, KeyDecoName>& GetKeys() const {
    return mMapKeys;
  }
  void focusItem(const PropTypeString& key);

  bool isKeyPresent(const KeyDecoName& pkey) const;
  void addKey(const KeyDecoName& pkey);

  bool isMultiMap() const {
    return mbIsMultiMap;
  }

private:
  orkmap<PropTypeString, KeyDecoName> mMapKeys;
  const reflect::IMap* mMapProp = nullptr;
  GedItemNode* mKeyNode         = nullptr;
  PropTypeString mCurrentKey;
  int mItemIndex    = 0;
  bool mbSingle     = false;
  bool mbConst      = false;
  bool mbIsMultiMap = false;
  bool mbImpExp     = false;

  bool OnMouseDoubleClicked(event_constptr_t ev) final;
  void DoDraw(Context* pTARG) final;

  void updateVisibility();
  void addItem(event_constptr_t ev);
  void removeItem(event_constptr_t ev);
  void moveItem(event_constptr_t ev);
  void duplicateItem(event_constptr_t ev);
  void importItem(event_constptr_t ev);
  void exportItem(event_constptr_t ev);
};

///////////////////////////////////////////////////////////////////////////////

struct GedArrayNode : public GedItemNode {

  DeclareAbstractX(GedArrayNode, GedItemNode);

public:
  using event_constptr_t = ork::ui::event_constptr_t;

  GedArrayNode(GedContainer* c, const char* name, const reflect::IArray* prop, object_ptr_t obj);
  /*const orkmap<PropTypeString, KeyDecoName>& GetKeys() const {
    return mMapKeys;
  }
  void focusItem(const PropTypeString& key);

  bool isKeyPresent(const KeyDecoName& pkey) const;
  void addKey(const KeyDecoName& pkey);

  bool isMultiMap() const {
    return mbIsMultiMap;
  }*/

private:
  //orkmap<PropTypeString, KeyDecoName> mMapKeys;
  const reflect::IArray* _arrayProperty = nullptr;
  //GedItemNode* mKeyNode         = nullptr;
  //PropTypeString mCurrentKey;
  int _selectedItemIndex    = 0;
  bool _isSingle     = false;
  bool _isConst      = false;
  //bool mbIsMultiMap = false;
  //bool mbImpExp     = false;

  bool OnMouseDoubleClicked(event_constptr_t ev) final;
  void DoDraw(Context* pTARG) final;
  void updateVisibility();
  //void addItem(event_constptr_t ev);
  //void removeItem(event_constptr_t ev);
  //void moveItem(event_constptr_t ev);
  //void duplicateItem(event_constptr_t ev);
  //void importItem(event_constptr_t ev);
  //void exportItem(event_constptr_t ev);
};

///////////////////////////////////////////////////////////////////////////////

struct GedObjNode : public GedItemNode {
  DeclareAbstractX(GedObjNode, GedItemNode);

public:
  GedObjNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj);

private:
  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
  void DoDraw(lev2::Context* pTARG) final; 

  bool mbInteractive;
  bool mbCollapse;
};

///////////////////////////////////////////////////////////////////////////////

struct GedFactoryNode : public GedItemNode {
  DeclareAbstractX(GedFactoryNode, GedItemNode);

public:
  GedFactoryNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual
  bool OnMouseDoubleClicked(ui::event_constptr_t ev) final;

  bool mbInteractive;
  bool mbCollapse;
  factory_class_set_t _factory_set;

};

///////////////////////////////////////////////////////////////////////////////

struct GedBoolNode : public GedItemNode {
  DeclareAbstractX(GedBoolNode, GedItemNode);

public:
  GedBoolNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual
  bool OnMouseReleased(ork::ui::event_constptr_t ev) final;
  bool OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

};

///////////////////////////////////////////////////////////////////////////////
class SliderBase {
public:
  virtual ~SliderBase() {
  }
  virtual void resize(int ix, int iy, int iw, int ih)  = 0;
  virtual bool OnUiEvent(ork::ui::event_constptr_t ev) = 0;

  void SetLogMode(bool bv) {
    mlogmode = bv;
  }
  float GetIndicPos() const {
    return mfIndicPos;
  }
  float GetTextPos() const {
    return mfTextPos;
  }
  void SetIndicPos(float fi) {
    mfIndicPos = fi;
  }
  void SetTextPos(float ti) {
    mfTextPos = ti;
  }
  PropTypeString& ValString() {
    return mValStr;
  }
  void SetLabelH(int ilabh) {
    miLabelH = ilabh;
  }
  void onActivate() {
    OrkAssert(false);
  }
  void onDeactivate() {
    OrkAssert(false);
  }

  newiodriver_ptr_t _iodriver;

protected:
  bool mlogmode    = false;
  int miLabelH     = 0;
  float mfx        = 0.0f;
  float mfw        = 0.0f;
  float mfh        = 0.0f;
  float mfTextPos  = 0.0f;
  float mfIndicPos = 0.0f;
  PropTypeString mValStr;
};
///////////////////////////////////////////////////////////////////////////////
template <typename T> class Slider : public SliderBase {
public:
  using datatype = T;

  Slider(GedItemNode*  ParentW, datatype min, datatype max, datatype def);

  bool OnUiEvent(ork::ui::event_constptr_t ev) final;

  void resize(int ix, int iy, int iw, int ih) final;
  void SetVal(datatype val);
  void Refresh();

  void SetMinMax(datatype min, datatype max) {
    mmin = min;
    mmax = max;
    if(mmax==mmin){
      mmax = mmin+T(1);
    }
  }

  datatype value() const { return mval; }

private:
  GedItemNode* _parent = nullptr;
  datatype mval;
  datatype mmin;
  datatype mmax;
  bool mbUpdateOnDrag;

  float LogToVal(float logval) const;
  float ValToLog(float val) const;
  float LinToVal(float linval) const;
  float ValToLin(float val) const;
};

using sliderbase_ptr_t = std::shared_ptr<SliderBase>;
///////////////////////////////////////////////////////////////////////////////

struct GedIntNode : public GedItemNode {
  using datatype = int;
  DeclareAbstractX(GedIntNode, GedItemNode);

public:
  GedIntNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual

  bool OnUiEvent(ork::ui::event_constptr_t ev) final;

  sliderbase_ptr_t _slider;
  bool _is_log_mode = false;
};
///////////////////////////////////////////////////////////////////////////////

struct GedFloatNode : public GedItemNode {
  using datatype = float;
  DeclareAbstractX(GedFloatNode, GedItemNode);

public:
  GedFloatNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual

  bool OnUiEvent(ork::ui::event_constptr_t ev) final;

  sliderbase_ptr_t _slider;
  bool _is_log_mode = false;
};

///////////////////////////////////////////////////////////////////////////////

struct GedCurve1DNode : public GedItemNode {
  DeclareAbstractX(GedCurve1DNode, GedItemNode);

public:
  GedCurve1DNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual

  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
  int doComputeHeight() const final;

};

///////////////////////////////////////////////////////////////////////////////

struct GedGradientNode : public GedItemNode {
  DeclareAbstractX(GedGradientNode, GedItemNode);

public:
  GedGradientNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

  void DoDraw(lev2::Context* pTARG) final; // virtual

  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
  int doComputeHeight() const final;
};

///////////////////////////////////////////////////////////////////////////////

struct GedAssetNode : public GedItemNode {
  DeclareAbstractX(GedAssetNode, GedItemNode);

public:
  ///////////////////////////////////////////////////

  GedAssetNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

private:
  void DoDraw(lev2::Context* pTARG) final;
  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
};

///////////////////////////////////////////////////////////////////////////////

struct GedPlugNode : public GedItemNode {
  DeclareAbstractX(GedPlugNode, GedItemNode);

public:
  ///////////////////////////////////////////////////

  GedPlugNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

private:
  int doComputeHeight() const final;
  void DoDraw(lev2::Context* pTARG) final;
  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
};

///////////////////////////////////////////////////////////////////////////////

struct GedEnumNode : public GedItemNode {
  DeclareAbstractX(GedEnumNode, GedItemNode);

public:
  ///////////////////////////////////////////////////

  GedEnumNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

private:

  void DoDraw(lev2::Context* pTARG) final;
  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
};

///////////////////////////////////////////////////////////////////////////////

struct GedColorNode : public GedItemNode {
  DeclareAbstractX(GedColorNode, GedItemNode);

public:
  ///////////////////////////////////////////////////

  GedColorNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver);

private:

  void DoDraw(lev2::Context* pTARG) final;
  bool OnUiEvent(ork::ui::event_constptr_t ev) final;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
