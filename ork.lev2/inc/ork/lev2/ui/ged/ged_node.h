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
  virtual int computeHeight();
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
  std::string _propname;
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
  int micalch          = 0;

  newiodriver_ptr_t _iodriver;
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
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;
  ork::file::Path::NameType mPersistID;

  GedGroupNode(
      GedContainer* container,             //
      const char* name,                    //
      const reflect::ObjectProperty* prop, //
      object_ptr_t obj,                    //
      bool is_obj_node = false);

  void updateVisibility();
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

  void OnMouseDoubleClicked(event_constptr_t ev) final;
  void DoDraw(Context* pTARG) final;

  void updateVisibility();
  void addItem(event_constptr_t ev);
  void removeItem(event_constptr_t ev);
  void moveItem(event_constptr_t ev);
  void duplicateItem(event_constptr_t ev);
  void importItem(event_constptr_t ev);
  void exportItem(event_constptr_t ev);
};

struct GedObjNode : public GedItemNode {
  DeclareAbstractX(GedObjNode, GedItemNode);

public:
  GedObjNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj);
  
  //void OnCreateObject();
  //void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

  void DoDraw(lev2::Context* pTARG) final; // virtual
private:
  //Setter mSetter;
  bool mbInteractive;
  bool mbCollapse;
};

///////////////////////////////////////////////////////////////////////////////

struct GedFactoryNode : public GedItemNode {
  DeclareAbstractX(GedFactoryNode, GedItemNode);
public:
  GedFactoryNode( GedContainer* c, 
                  const char* name, 
                  newiodriver_ptr_t iodriver);
  

  void DoDraw(lev2::Context* pTARG) final; // virtual
  void OnMouseDoubleClicked(ui::event_constptr_t ev) final;

  bool mbInteractive;
  bool mbCollapse;
  factory_class_set_t _factory_set;
  newiodriver_ptr_t _iodriver;
};

///////////////////////////////////////////////////////////////////////////////

struct GedBoolNode : public GedItemNode {
  DeclareAbstractX(GedBoolNode, GedItemNode);
public:
  GedBoolNode( GedContainer* c, 
               const char* name, 
               newiodriver_ptr_t iodriver);
  

  void DoDraw(lev2::Context* pTARG) final; // virtual
  void OnMouseReleased(ork::ui::event_constptr_t ev) final;
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

  newiodriver_ptr_t _iodriver;
};

///////////////////////////////////////////////////////////////////////////////
class SliderBase {
public:
  virtual ~SliderBase() {}
  virtual void resize(int ix, int iy, int iw, int ih)  = 0;
  virtual void OnUiEvent(ork::ui::event_constptr_t ev) = 0;

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

protected:
  bool mlogmode = false;
  int miLabelH = 0;
  float mfx = 0.0f;
  float mfw = 0.0f;
  float mfh = 0.0f;
  float mfTextPos = 0.0f;
  float mfIndicPos = 0.0f;
  PropTypeString mValStr;
};
///////////////////////////////////////////////////////////////////////////////
template <typename T> class Slider : public SliderBase {
public:
  typedef typename T::datatype datatype;

  Slider(T& ParentW, datatype min, datatype max, datatype def);

  void OnUiEvent(ork::ui::event_constptr_t ev) final;

  void resize(int ix, int iy, int iw, int ih) final;
  void SetVal(datatype val);
  void Refresh();

  void SetMinMax(datatype min, datatype max) {
    mmin = min;
    mmax = max;
  }

private:
  T& _parent;
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
  GedIntNode( GedContainer* c, 
               const char* name, 
               newiodriver_ptr_t iodriver);
  

  void DoDraw(lev2::Context* pTARG) final; // virtual
  
  void OnUiEvent(ork::ui::event_constptr_t ev) final;

  newiodriver_ptr_t _iodriver;
  sliderbase_ptr_t _slider;
  bool _is_log_mode = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
