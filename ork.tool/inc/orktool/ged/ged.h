////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <orktool/orktool_pch.h>
#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/object/AutoConnector.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/any.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/ui/viewport.h>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialog>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
class Object;
namespace lev2 {
class Font;
}
namespace ui {
struct Event;
}
namespace tool {
class ChoiceManager;
template <typename T> class PickBuffer;
namespace ged {
///////////////////////////////////////////////////////////////////////////////
class GedWidget;
class GedItemNode;
class GedSerializer;
class GedWidget;
class GedItemNode;
static const int kmaxgedstring = 256;

void ClearObjSet();
void AddToObjSet(void* pobj);
bool IsObjInSet(void* pobj);

///////////////////////////////////////////////////////////////////////////////

struct PersistHashContext {
  ork::Object* mObject;
  const reflect::ObjectProperty* mProperty;
  const char* mString;

  PersistHashContext();
  int GenerateHash() const;
};

class PersistantMap : public ork::Object {
  RttiDeclareConcrete(PersistantMap, ork::Object);

  orklut<std::string, std::string> mProperties;

public:
  const std::string& GetValue(const std::string& key);
  void SetValue(const std::string& key, const std::string& val);

  PersistantMap();
  ~PersistantMap();
};

///////////////////////////////////////////////////////////////////////////////

class PersistMapContainer : public ork::Object {
  RttiDeclareConcrete(PersistMapContainer, ork::Object);

  orklut<int, PersistantMap*> mPropPersistMap;

public:
  PersistMapContainer();
  ~PersistMapContainer();

  orklut<int, PersistantMap*>& GetMap() {
    return mPropPersistMap;
  }
  const orklut<int, PersistantMap*>& GetMap() const {
    return mPropPersistMap;
  }

  void CloneFrom(const PersistMapContainer& oth);
};

///////////////////////////////////////////////////////////////////////////////

class ObjModel : public ork::AutoConnector {
  RttiDeclareConcrete(ObjModel, ork::AutoConnector);

public:
  static orkset<ObjModel*> gAllObjectModels;
  static void FlushAllQueues();

  void SetGedWidget(GedWidget* Gedw) {
    mpGedWidget = Gedw;
  }
  void Attach(ork::Object* obj, bool bclearstack = true, GedItemNode* rootw = 0);
  GedItemNode* Recurse(ork::Object* obj, const char* pname = 0, bool binline = false);
  void Detach();
  GedWidget* GetGedWidget() const {
    return mpGedWidget;
  }
  ObjModel();
  /*virtual*/ ~ObjModel();

  void SetChoiceManager(util::choicemanager_ptr_t chcman) {
    mChoiceManager = chcman;
  }
  util::choicemanager_ptr_t GetChoiceManager(void) const {
    return mChoiceManager;
  }

  void Dump(const char* header) const;

  void QueueObject(ork::Object* obj) {
    mQueueObject = obj;
  }

  void ProcessQueue();

  ork::Object* CurrentObject() const {
    return mCurrentObject;
  }

  PersistantMap* GetPersistMap(const PersistHashContext& ctx);

  void QueueUpdateAll();
  void QueueUpdate();

  void FlushQueue();

  void PushBrowseStack(ork::Object* pobj);
  void PopBrowseStack();
  ork::Object* BrowseStackTop() const;
  int StackSize() const;

  PersistMapContainer& GetPersistMapContainer() {
    return mPersistMapContainer;
  }
  const PersistMapContainer& GetPersistMapContainer() const {
    return mPersistMapContainer;
  }

  void EnablePaint() {
    mbEnablePaint = true;
  }

private:
  GedWidget* mpGedWidget;
  ork::Object* mCurrentObject;
  ork::Object* mRootObject;
  ork::Object* mQueueObject;
  util::choicemanager_ptr_t mChoiceManager;
  orkstack<ork::Object*> mBrowseStack;
  PersistMapContainer mPersistMapContainer;
  bool mbEnablePaint;

  //////////////////////////////////////////////////////////

  DeclarePublicSignal(Repaint);
  DeclarePublicSignal(PreNewObject);
  DeclarePublicSignal(ModelInvalidated);
  DeclarePublicSignal(PropertyInvalidated);
  DeclarePublicSignal(NewObject);
  DeclarePublicSignal(SpawnNewGed);

  DeclarePublicSignal(PostNewObject);

  DeclarePublicAutoSlot(NewObject);
  DeclarePublicAutoSlot(RelayModelInvalidated);
  DeclarePublicAutoSlot(RelayPropertyInvalidated);
  DeclarePublicAutoSlot(ObjectDeleted);
  DeclarePublicAutoSlot(ObjectSelected);
  DeclarePublicAutoSlot(ObjectDeSelected);
  DeclarePublicAutoSlot(Repaint);

  reflect::IInvokation* mModelInvalidatedInvoker;

public:
  void SigModelInvalidated();
  void SigPreNewObject();
  void SigPropertyInvalidated(ork::Object* pobj, const reflect::ObjectProperty* prop);
  void SigRepaint();
  void SigSpawnNewGed(ork::Object* pobj);
  void SigNewObject(ork::Object* pobj);
  void SigPostNewObject(ork::Object* pobj);

private:
  //////////////////////////////////////////////////////////

  void SlotNewObject(ork::Object* pobj);
  void SlotRelayModelInvalidated();
  void SlotRelayPropertyInvalidated(ork::Object* pobj, const reflect::ObjectProperty* prop);
  void SlotObjectDeleted(ork::Object* pobj);
  void SlotObjectSelected(ork::Object* pobj);
  void SlotObjectDeSelected(ork::Object* pobj);
  void SlotRepaint();

  //////////////////////////////////////////////////////////

  struct sortnode {
    std::string Name;
    orkvector<std::pair<std::string, reflect::ObjectProperty*>> PropVect;
    orkvector<std::pair<std::string, sortnode*>> GroupVect;
  };

  void EnumerateNodes(sortnode& in_node, object::ObjectClass*);

  //////////////////////////////////////////////////////////

  bool IsNodeVisible(const reflect::ObjectProperty* prop);
  GedItemNode* CreateNode(const std::string& Name, const reflect::ObjectProperty* prop, Object* pobject);
};
///////////////////////////////////////////////////////////////////////////////
class GedFactory : public ork::Object {
  RttiDeclareAbstract(GedFactory, ork::Object);

public:
  virtual void Recurse(ObjModel& mdl, const reflect::ObjectProperty* prop, ork::Object* pobj) const {
  }
  virtual GedItemNode*
  CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const;
};
///////////////////////////////////////////////////////////////////////////////

class GedObject;
class GedSurface;

class GedSkin {

public:
  typedef void (*DrawCB)(GedSkin* pskin, GedObject* pnode, ork::lev2::Context* pTARG);

  void gpuInit(lev2::Context* ctx);

  ork::lev2::freestyle_mtl_ptr_t _material;
  const ork::lev2::FxShaderTechnique* _tekpick     = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxpick  = nullptr;
  const ork::lev2::FxShaderTechnique* _tekmodcolor = nullptr;
  const ork::lev2::FxShaderParam* _parmvp          = nullptr;
  const ork::lev2::FxShaderParam* _parmodcolor     = nullptr;
  const ork::lev2::FxShaderParam* _parobjid        = nullptr;

  struct GedPrim {
    DrawCB mDrawCB;
    GedObject* mpNode;
    int ix1, iy1, ix2, iy2;
    fvec4 _ucolor;
    ork::lev2::PrimitiveType meType;
    int miSortKey;

    GedPrim()
        : mDrawCB(0)
        , mpNode(0)
        , ix1(0)
        , ix2(0)
        , iy1(0)
        , iy2(0)
        , _ucolor(0)
        , miSortKey(0)
        , meType(ork::lev2::PrimitiveType::END) {
    }
  };

  struct PrimContainer {
    static const int kmaxprims    = 8192;
    static const int kmaxprimsper = 4096;
    fixed_pool<GedPrim, kmaxprims> mPrimPool;
    fixedvector<GedPrim*, kmaxprimsper> mLinePrims;
    fixedvector<GedPrim*, kmaxprimsper> mQuadPrims;
    fixedvector<GedPrim*, kmaxprimsper> mCustomPrims;

    void clear();
  };

  GedSkin();

  typedef enum {
    ESTYLE_BACKGROUND_1 = 0,
    ESTYLE_BACKGROUND_2,
    ESTYLE_BACKGROUND_3,
    ESTYLE_BACKGROUND_4,
    ESTYLE_BACKGROUND_5,
    ESTYLE_BACKGROUND_OPS,
    ESTYLE_BACKGROUND_GROUP_LABEL,
    ESTYLE_BACKGROUND_MAPNODE_LABEL,
    ESTYLE_BACKGROUND_OBJNODE_LABEL,
    ESTYLE_DEFAULT_HIGHLIGHT,
    ESTYLE_DEFAULT_OUTLINE,
    ESTYLE_DEFAULT_CAPTION,
    ESTYLE_DEFAULT_CHECKBOX,
    ESTYLE_BUTTON_OUTLINE,
  } ESTYLE;

  virtual void Begin(ork::lev2::Context* pTARG, GedSurface* pgedvp)                                       = 0;
  virtual void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0)      = 0;
  virtual void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0) = 0;
  virtual void DrawLine(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                      = 0;
  virtual void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih)                             = 0;
  virtual void DrawDownArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                 = 0;
  virtual void DrawRightArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                = 0;
  virtual void DrawText(GedObject* pnode, int ix, int iy, const char* ptext)                              = 0;
  virtual void End(ork::lev2::Context* pTARG)                                                             = 0;

  void SetScrollY(int iscrolly) {
    miScrollY = iscrolly;
  }

  void AddPrim(const GedPrim& cb); // { mPrims.AddSorted(calcsort(cb.miSortKey),cb); }

  int GetScrollY() const {
    return miScrollY;
  }

  int calcsort(int isort) {
    int ioutsort = (isort << 16); //+int(mPrims.size());
    return ioutsort;
  }

  static const int kMaxPrimContainers = 32;
  fixed_pool<PrimContainer, 32> mPrimContainerPool;
  typedef fixedlut<int, PrimContainer*, kMaxPrimContainers> PrimContainers;

  void clear();

  int char_w() const {
    return miCHARW;
  }
  int char_h() const {
    return miCHARH;
  }

protected:
  PrimContainers mPrimContainers;

  int miScrollY;
  int miRejected;
  int miAccepted;
  GedSurface* mpCurrentGedVp;
  ork::lev2::Font* mpFONT;
  int miCHARW;
  int miCHARH;

  bool IsVisible(ork::lev2::Context* pTARG, int iy1, int iy2) {
    int iry1 = iy1 + miScrollY;
    int iry2 = iy2 + miScrollY;
    int ih   = pTARG->mainSurfaceHeight();

    if (iry2 < 0) {
      miRejected++;
      return false;
    } else if (iry1 > ih) {
      miRejected++;
      return false;
    }

    miAccepted++;
    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////

class GedObject : public ork::Object {

protected:
  int miD;
  int miDecoIndex;

  GedObject()
      : miD(0)
      , miDecoIndex(0) {
  }

public:
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

class GedItemNode : public GedObject {
  RttiDeclareAbstract(GedItemNode, GedObject);

public:
  ///////////////////////////////////////////////////

  GedItemNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);

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

  void activate() {
    onActivate();
  }
  void deactivate() {
    onDeactivate();
  }
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
  virtual void onActivate() {
  }
  virtual void onDeactivate() {
  }
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
  void AddItem(GedItemNode* w);
  GedItemNode* GetItem(int idx) const;
  int GetNumItems() const;
  GedItemNode* GetParent() const {
    return _parent;
  }
  ///////////////////////////////////////////////////
  virtual int CalcHeight(void);
  GedItemNode* GetChildContainer() {
    return this;
  }
  ///////////////////////////////////////////////////
  void SetOrkProp(const reflect::ObjectProperty* prop) {
    mOrkProp = prop;
  }
  const reflect::ObjectProperty* GetOrkProp() const {
    return mOrkProp;
  }
  ///////////////////////////////////////////////////
  void SetOrkObj(ork::Object* obj) {
    mOrkObj = obj;
  }
  ork::Object* GetOrkObj() const {
    return mOrkObj;
  }
  ///////////////////////////////////////////////////
  GedWidget* GetGedWidget() const {
    return mRoot;
  }
  ///////////////////////////////////////////////////
  bool IsObjectHilighted(const GedObject* pobj) const;
  ///////////////////////////////////////////////////
  GedSkin* GetSkin() const;
  ///////////////////////////////////////////////////
  const reflect::ObjectProperty* mOrkProp;
  ork::Object* mOrkObj;
  static GedSkin* gpSkin0;
  static GedSkin* gpSkin1;
  static int giSkin;

  virtual void DoDraw(lev2::Context* pTARG) = 0;

  int miX, miY;
  int miW, miH;
  bool mbVisible;
  bool mbInvalid;

  typedef std::string NameType;

  int get_charh() const;
  int get_charw() const;
  int get_text_center_y() const;
  bool mbcollapsed;
  ork::ArrayString<kmaxgedstring> mvalue;
  orkvector<GedItemNode*> mItems;
  GedItemNode* _parent;
  orkmap<std::string, std::string> mTags;
  GedWidget* mRoot;
  int micalch;
  ///////////////////////////////////////////////////
  std::string _propname;
  std::string _content;
  ///////////////////////////////////////////////////
  ObjModel& mModel;
};

///////////////////////////////////////////////////////////////////////////////

class GedLabelNode : public GedItemNode {
public:
  ///////////////////////////////////////////////////

  GedLabelNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
      : GedItemNode(mdl, name, prop, obj) {
  }

private:
  virtual void DoDraw(lev2::Context* pTARG);
};

///////////////////////////////////////////////////////////////////////////////
class GedRootNode : public GedItemNode {
  virtual void DoDraw(lev2::Context* pTARG);
  virtual void Layout(int ix, int iy, int iw, int ih);
  virtual int CalcHeight(void);

public:
  GedRootNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);
  bool DoDrawDefault() const {
    return false;
  } // virtual
};
///////////////////////////////////////////////////////////////////////////////
class GedGroupNode : public GedItemNode {
  virtual void DoDraw(lev2::Context* pTARG);
  bool mbCollapsed;
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;
  ork::file::Path::NameType mPersistID;

public:
  GedGroupNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj, bool is_obj_node = false);

  void CheckVis();
  bool DoDrawDefault() const {
    return false;
  } // virtual
  bool mIsObjNode;
};
///////////////////////////////////////////////////////////////////////////////
class GedSurface;

class GedWidget : public ork::AutoConnector {
  RttiDeclareAbstract(GedWidget, ork::AutoConnector);

  static const int kdim = 8;

  GedItemNode* mRootItem;
  int miW;
  int miH;
  ork::Object* mRootObject;
  ObjModel& mModel;
  std::deque<GedItemNode*> mItemStack;
  int miRootH;
  GedSurface* mViewport;
  U64 mStackHash;
  orkvector<GedSkin*> mSkins;
  int miSkin;
  bool mbDeleteModel;

  void ComputeStackHash();

  //////////////////////////////////////////////////////////////

  DeclarePublicSignal(Repaint);
  DeclarePublicAutoSlot(Repaint);
  DeclarePublicAutoSlot(ModelInvalidated);

  void SlotRepaint();
  void SlotModelInvalidated();

  //////////////////////////////////////////////////////////////

public:
  void SetDeleteModel(bool bv) {
    mbDeleteModel = true;
  }

  void PropertyInvalidated(ork::Object* pobj, const reflect::ObjectProperty* prop);

  GedWidget(ObjModel& model);
  ~GedWidget();
  void Attach(ork::Object* obj);

  void Draw(lev2::Context* pTARG, int iw, int ih, int iscrolly);

  ObjModel& GetModel() {
    return mModel;
  }

  GedItemNode* GetRootItem() const {
    return mRootItem;
  }

  void IncrementSkin();
  GedItemNode* ParentItemNode() const;
  void PushItemNode(GedItemNode* qw);
  void PopItemNode(GedItemNode* qw);
  void AddChild(GedItemNode* pw);
  void DoResize();
  void OnSelectionChanged();
  int GetStackDepth() const {
    return int(mItemStack.size());
  }
  int GetRootHeight() const {
    return miRootH;
  }

  void setViewport(GedSurface* pvp) {
    mViewport = pvp;
  }
  U64 GetStackHash() const;

  GedSurface* GetViewport() const {
    return mViewport;
  }
  GedSkin* GetSkin();
  void AddSkin(GedSkin* psk);
  void SetDims(int iw, int ih);
};
///////////////////////////////////////////////////////////////////////////////
class GedSurface : public ui::Surface {
public:
  // friend class lev2::PickBuffer<GedSurface>;

  fvec4 AssignPickId(GedObject* pobj);
  GedWidget& GetGedWidget() {
    return mWidget;
  }
  GedSurface(const std::string& name, ObjModel& model);
  ~GedSurface();

  void ResetScroll() {
    miScrollY = 0;
  }

  const GedObject* GetMouseOverNode() const {
    return mpMouseOverNode;
  }

  static orkset<GedSurface*> gAllViewports;
  void SetDims(int iw, int ih);
  void onInvalidate();

private:
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoSurfaceResize() override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  void DoInit(lev2::Context* pt) override;

  ObjModel& mModel;
  GedWidget mWidget;
  GedObject* mpActiveNode;
  int miScrollY;
  const GedObject* mpMouseOverNode;
  ork::msgrouter::subscriber_t _simulation_subscriber;
};
///////////////////////////////////////////////////////////////////////////////
class GedTextEdit : public QLineEdit {
  Q_OBJECT

public:
  GedTextEdit(QWidget* parent);
  void focusOutEvent(QFocusEvent* pev) final; // virtual
  void keyPressEvent(QKeyEvent* pev) final;   // virtual
  void _setText(const char* ptext);

signals:
  void editFinished();
  void canceled();
};

class GedInputDialog : public QDialog {
  Q_OBJECT
public:
  GedInputDialog();

  static QString getText(ork::ui::event_constptr_t ev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih);
  bool wasChanged() const {
    return mbChanged;
  }
  QString getResult();
  void clear() {
    mTextEdit.clear();
  }
  GedTextEdit mTextEdit;
  QString mResult;
  bool mbChanged;

public slots:

  void canceled();
  void accepted();
  void textChanged(QString str);
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ged
} // namespace tool
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
