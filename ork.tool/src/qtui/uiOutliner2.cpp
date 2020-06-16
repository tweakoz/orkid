#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/reflect/Functor.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <orktool/ged/ged.h>
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/EditorCamera.h>

#include "uiOutliner2.h"

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/qtui/qtui.hpp>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Outliner2Model, "Outliner2Model");

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

typedef SVtxV16T16C16 vtx_t;

void Outliner2Model::Describe() {
  RegisterAutoSlot(Outliner2Model, SceneTopoChanged);
  RegisterAutoSlot(Outliner2Model, ObjectSelected);
  RegisterAutoSlot(Outliner2Model, ObjectDeSelected);
  RegisterAutoSlot(Outliner2Model, ClearSelection);

  RegisterAutoSignal(Outliner2Model, ModelChanged);
}

///////////////////////////////////////////////////////////////////////////////

Outliner2Model::Outliner2Model(SceneEditorBase& ed, Outliner2Surface& v)
    : mEditor(ed)
    , ConstructAutoSlot(SceneTopoChanged)
    , ConstructAutoSlot(ObjectSelected)
    , ConstructAutoSlot(ObjectDeSelected)
    , ConstructAutoSlot(ClearSelection)
    , mVP(v)
    , mLastSelection(-1)
    , mShowArchs(true)
    , mShowEnts(true)
    , mShowComps(true)
    , mShowSystems(true) {
  SetupSignalsAndSlots();
}

///////////////////////////////////////////////////////////////////////////////

Outliner2Model::~Outliner2Model() {
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::IncSel() {
  auto& selmgr = mEditor.selectionManager();

  ork::Object* pobj = nullptr;

  mLastSelection++;
  if (mLastSelection >= mItems.size())
    mLastSelection = 0;

  if (mLastSelection < mItems.size()) {
    pobj = mItems[mLastSelection].mObject;
    selmgr.ClearSelection();
    selmgr.AddObjectToSelection(pobj);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::DecSel() {
  auto& selmgr = mEditor.selectionManager();

  ork::Object* pobj = nullptr;

  mLastSelection--;
  if (mLastSelection < 0)
    mLastSelection = mItems.size() - 1;

  if (mLastSelection >= 0) {
    pobj = mItems[mLastSelection].mObject;
    selmgr.ClearSelection();
    selmgr.AddObjectToSelection(pobj);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::ToggleEnts() {
  mShowEnts = !mShowEnts;
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::ToggleArchs() {
  mShowArchs = !mShowArchs;
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::ToggleComps() {
  mShowComps = !mShowComps;
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::ToggleSystems() {
  mShowSystems = !mShowSystems;
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::ToggleGlobals() {
  mShowGlobals = !mShowGlobals;
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::UpdateModel() {
  // printf("Outliner2Model<%p>::SlotSceneTopoChanged\n", this);

  mItems.clear();

  auto scene_data = mEditor.GetSceneData();

  if (scene_data) {
    int iy         = 0;
    bool alt       = false;
    int index      = 0;
    mLastSelection = -1;
    ///////////////////////////////////////////////
    // add single item
    ///////////////////////////////////////////////
    auto additem = [&](const char* name, ork::Object* obj, int indent) {
      Outliner2Item o2i;

      o2i.mName   = name;
      o2i.mObject = obj;
      o2i.mIndent = indent;

      bool is_sel = mSelected.find((ork::Object*)obj) != mSelected.end();

      if (is_sel)
        mLastSelection = index;

      o2i.mSelected = is_sel;
      mItems.push_back(o2i);
      index++;
    };
    ///////////////////////////////////////////////
    if (mShowGlobals)
      additem("SceneGlobal", scene_data, 0);
    ///////////////////////////////////////////////
    // sceneobjects
    ///////////////////////////////////////////////
    orkmap<PoolString, SceneObject*>& objs = scene_data->GetSceneObjects();
    size_t numobjs                         = objs.size();
    ///////////////////////////////////////////////
    for (const auto& item : objs) {
      const PoolString& name = item.first;
      SceneObject* pobj      = item.second;
      Archetype* as_arch     = rtti::autocast(pobj);
      EntData* as_ent        = rtti::autocast(pobj);
      ///////////////////////////////////////////////
      if (as_ent && false == mShowEnts)
        pobj = nullptr;
      if (as_arch && false == mShowArchs)
        pobj = nullptr;
      ///////////////////////////////////////////////
      if (pobj) {
        FixedString<256> decnam;
        if (as_arch)
          decnam.format("(a) %s", item.first.c_str());
        if (as_ent)
          decnam.format("(e) %s", item.first.c_str());

        additem(decnam.c_str(), pobj, 0);

        //////////////////////////////////////
        // if its an archetype
        //  descend into the archetypes components
        //////////////////////////////////////

        if (as_arch && mShowComps) {

          const auto& comps = as_arch->GetComponentDataTable().GetComponents();
          for (const auto& citem : comps) {
            auto c          = citem.second;
            auto clazz      = c->GetClass();
            auto class_name = clazz->Name().c_str();
            decnam.format("(c) %s", class_name);
            additem(decnam.c_str(), c, 1);
          }
        }

        //////////////////////////////////////
      }
    }

    ///////////////////////////////////////////////
    // systemdatas
    ///////////////////////////////////////////////

    if (mShowSystems)
      for (auto item : scene_data->getSystemDatas()) {
        const PoolString& name = item.first;
        SystemData* sysdat     = item.second;
        if (sysdat != nullptr) {
          FixedString<256> decnam;
          decnam.format("(s) %s", name.c_str());
          additem(decnam.c_str(), sysdat, 0);
        }
      }

    ///////////////////////////////////////////////
  }

  mVP.SetDirty();

  SigModelChanged();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::SlotSceneTopoChanged() {
  UpdateModel();
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::SigModelChanged() {
  // mSignalModelChanged(&Outliner2Model::SigModelChanged);
}

///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotObjectSelected(ork::Object* pobj) {
  // printf("Outliner2Model<%p>::SlotObjectSelected obj<%p>\n", this, pobj);
  mSelected.insert(pobj);
  UpdateModel();
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotObjectDeSelected(ork::Object* pobj) {
  // printf("Outliner2Model<%p>::SlotObjectDeSelected obj<%p>\n", this, pobj);
  auto it = mSelected.find(pobj);
  if (it != mSelected.end()) {
    mSelected.erase(it);
  }
  UpdateModel();
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotClearSelection() {
  mSelected.clear();
  UpdateModel();
}
///////////////////////////////////////////////////////////////////////////////

Outliner2Surface::Outliner2Surface(SceneEditorBase& ed)
    : ui::Surface("outl2", 0, 0, 0, 0, fcolor3::Black(), 0.0f)
    , mOutlinerModel(ed, *this)
    , mFont(nullptr)
    , mCtxBase(nullptr)
    , miScrollY(0)
    , mContentH(0)
    , ConstructAutoSlot(ModelChanged)
    , mDark(true) {
  object::Connect(&ed.GetSigSceneTopoChanged(), &mOutlinerModel.GetSlotSceneTopoChanged());
  object::Connect(&mOutlinerModel.GetSigModelChanged(), &this->GetSlotModelChanged());
  object::Connect(&ed.selectionManager().GetSigObjectSelected(), &mOutlinerModel.GetSlotObjectSelected());
  object::Connect(&ed.selectionManager().GetSigObjectDeSelected(), &mOutlinerModel.GetSlotObjectDeSelected());
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Surface::SlotObjectSelected(ork::Object* pobj) {
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Surface::SlotObjectDeSelected(ork::Object* pobj) {
}

///////////////////////////////////////////////////////////////////////////////

void Outliner2Surface::SlotModelChanged() {
  assert(false);
}
///////////////////////////////////////////////////////////////////////////////
int Outliner2Surface::kitemh() const {
  return mCharH + 4;
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Surface::DoInit(lev2::Context* pt) {
  auto par    = pt->FBI()->GetThisBuffer();
  _pickbuffer = new lev2::PickBuffer(this, pt, width(), height());
  ///////////////////////////////////////////////
  mFont          = lev2::FontMan::GetFont("i13");
  auto& fontdesc = mFont->GetFontDesc();
  ///////////////////////////////////////////////
  mCharW = fontdesc.miAdvanceWidth;
  mCharH = fontdesc.miAdvanceHeight;
  ///////////////////////////////////////////////
  mCtxBase = pt->GetCtxBase();
  ///////////////////////////////////////////////
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(pt, "orkshader://ui2");
  _tekpick     = _material->technique("ui_picking");
  _tekmodcolor = _material->technique("ui_modcolor");
  _parmvp      = _material->param("mvp");
  _parmodcolor = _material->param("modcolor");
  _parobjid    = _material->param("objid");

  _material->dump();

  OrkAssert(_tekpick != nullptr);
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Surface::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  auto tgt = drwev->GetTarget();
  tgt->makeCurrentContext();
  tgt->debugPushGroup(FormatString("Outliner2Surface::repaint"));
  auto mtxi                            = tgt->MTXI();
  auto fbi                             = tgt->FBI();
  auto fxi                             = tgt->FXI();
  auto rsi                             = tgt->RSI();
  auto& primi                          = lev2::GfxPrimitives::GetRef();
  lev2::DynamicVertexBuffer<vtx_t>& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  SceneEditorBase& ed                  = mOutlinerModel.Editor();
  auto scene_data                      = ed.GetSceneData();
  bool has_foc                         = hasMouseFocus();
  bool is_pick                         = fbi->isPickState();

  // printf("Outliner2Surface<%p>::DoRePaintSurface is_pick<%d>\n", this, int(is_pick));

  //////////////////////////////////////////////////
  // Compute Scoll Transform
  //////////////////////////////////////////////////

  ork::fmtx4 matSCROLL;
  matSCROLL.SetTranslation(0.0f, float(miScrollY), 0.0f);
  lev2::SRasterState defstate;

  //////////////////////////////////////////////////

  fbi->pushScissor(ViewportRect(0, 0, width(), height()));
  fbi->pushViewport(ViewportRect(0, 0, width(), height()));

  {
    fbi->Clear(fvec4::Blue(), 1.0f);

    rsi->BindRasterState(defstate);

    const std::vector<Outliner2Item>& items = mOutlinerModel.Items();

    mContentH = items.size() * kitemh();

    fvec4 c1(0.7f, 0.7f, 0.8f);
    fvec4 c2(0.8f, 0.8f, 0.8f);
    fvec4 c3(0.8f, 0.0f, 0.0f);
    fvec4 col_sysdat(0.8f, 0.0f, 0.0f);
    fvec4 col_sysdat_alt(0.8f, 0.0f, 0.0f);
    fvec4 col_entity(0.8f, 0.0f, 0.0f);
    fvec4 col_entity_alt(0.8f, 0.0f, 0.0f);
    fvec4 col_archet(0.8f, 0.0f, 0.0f);
    fvec4 col_archet_alt(0.8f, 0.0f, 0.0f);
    fvec4 col_sceneglobal(0.8f, 0.0f, 0.0f);

    if (mDark) {
      c1              = fvec4(0.3f, 0.3f, 0.4f);
      c2              = fvec4(0.2f, 0.2f, 0.3f);
      c3              = fvec4(0.5f, 0.0f, 0.0f);
      col_sysdat      = fvec4(0.3f, 0.3f, 0.6f);
      col_sysdat_alt  = fvec4(0.2f, 0.2f, 0.5f);
      col_entity      = fvec4(0.2f, 0.4f, 0.2f);
      col_entity_alt  = fvec4(0.1f, 0.3f, 0.1f);
      col_archet      = fvec4(0.4f, 0.2f, 0.4f);
      col_archet_alt  = fvec4(0.3f, 0.1f, 0.3f);
      col_sceneglobal = fvec4(0.4f, 0.3f, 0.2f);
    }

    const int kheaderH = miScrollY;

    {
      int iy   = kheaderH;
      bool alt = false;
      RenderContextFrameData RCFD(tgt);
      //////////////////////////////////////
      if (is_pick)
        _material->begin(_tekpick, RCFD);
      else
        _material->begin(_tekmodcolor, RCFD);
      //////////////////////////////////////
      auto uimatrix = mtxi->uiMatrix(width(), height());
      _material->bindParamMatrix(_parmvp, uimatrix);
      //////////////////////////////////////

      for (const auto& item : items) {
        const std::string& name = item.mName;
        auto pobj               = item.mObject;
        fvec4 pick_color        = fvec4(1, 1, 0, 1);
        pick_color.SetRGBAU64(_pickbuffer->AssignPickId(pobj));
        bool is_sel = item.mSelected;

        if (is_pick) {
          pick_color = fvec4(100, 1, 0, 1); // come back as <0x0001 0000 0001 0064>
          _material->bindParamU64(_parobjid, uint64_t(pobj));
        } else {

          fvec4 modcolor;

          if (dynamic_cast<SceneData*>(pobj))
            modcolor = is_sel ? c3 : col_sceneglobal;
          else if (dynamic_cast<EntData*>(pobj))
            modcolor = is_sel ? c3 : (alt ? col_entity : col_entity_alt);
          else if (dynamic_cast<Archetype*>(pobj))
            modcolor = is_sel ? c3 : (alt ? col_archet : col_archet_alt);
          else if (dynamic_cast<SystemData*>(pobj))
            modcolor = is_sel ? c3 : (alt ? col_sysdat : col_sysdat_alt);
          else
            modcolor = is_sel ? c3 : (alt ? c1 : c2);

          _material->bindParamVec4(_parmodcolor, modcolor);
        }

        primi.RenderEMLQuadAtZV16T16C16(
            tgt,
            0,
            width(), // x0, x1
            iy,
            iy + kitemh(), // y0, y1
            0.0f,          // z
            0.0f,
            1.0f, // u0, u1
            0.0f,
            1.0f // v0, v1
        );

        iy += kitemh();
        alt = !alt;
      } // for (const auto& item : items) {

      _material->end(RCFD);

      //////////////////////////////////////

      if (false == is_pick) {
        mtxi->PushUIMatrix(width(), height());
        lev2::FontMan::PushFont(mFont);
        tgt->PushModColor(mDark ? fcolor4(0.7f, 0.7f, 0.8f) : fcolor4::Black());
        lev2::FontMan::beginTextBlock(tgt);
        iy = kheaderH + 5;
        for (const auto& item : items) {
          const std::string& name = item.mName;
          auto pobj               = item.mObject;
          int indent              = item.mIndent;

          lev2::FontMan::DrawText(tgt, (indent + 1) * 16, iy, name.c_str());
          iy += kitemh();
          alt = !alt;
        }
        lev2::FontMan::endTextBlock(tgt);
        lev2::FontMan::PopFont();
        tgt->PopModColor();
        mtxi->PopUIMatrix();
      }
    }
  }
  fbi->popViewport();
  fbi->popScissor();
  tgt->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Surface::SetNameOfSelectedItem() {
  int ilastsel = mOutlinerModel.GetLastSelection();

  int irx, iry;
  LocalToRoot(0, (ilastsel * kitemh()) + miScrollY, irx, iry);

  const std::vector<Outliner2Item>& items = mOutlinerModel.Items();

  if (ilastsel >= items.size())
    return;

  const Outliner2Item& item = items[ilastsel];

  // int irootx = mParent->miX;
  // int ipary = mParent->miY;

  auto g = mCtxBase->MapCoordToGlobal(fvec2(irx, iry));

  // QString qstr = tool::ged::GedInputDialog::getText ( &qev, & mParent, ptsg.c_str(), 2, 2, mParent.width()-3, iheight );
  tool::ged::GedInputDialog dialog;
  dialog.setModal(true);

  dialog.setGeometry(g.GetX(), g.GetY(), width(), kitemh());
  dialog.clear();
  dialog.mTextEdit.setGeometry(0, 0, width(), kitemh());
  dialog.mTextEdit._setText(item.mName.c_str());

  if (0 == dialog.exec() && dialog.wasChanged()) {
    auto result         = dialog.getResult();
    const char* rescstr = result.toStdString().c_str();
    auto& ed            = mOutlinerModel.Editor();
    auto& sm            = ed.selectionManager();
    // printf("rescstr<%s>\n", rescstr);
    ed.EditorRenameSceneObject((ent::SceneObject*)item.mObject, rescstr);
    sm.ClearSelection();
    sm.AddObjectToSelection((ork::Object*)item.mObject);
  }
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult Outliner2Surface::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);

  auto& ed           = mOutlinerModel.Editor();
  auto& sm           = ed.selectionManager();
  const auto& filtev = EV->mFilteredEvent;

  // ork::tool::ged::ObjModel::FlushAllQueues();
  int ix = EV->miX;
  int iy = EV->miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);

  lev2::PixelFetchContext ctx;
  ctx.miMrtMask = (1 << 0); // | (1 << 1); // ObjectID and ObjectUVD
  ctx.mUsage[0] = lev2::PixelFetchContext::EPU_PTR64;
  // ctx.mUsage[1] = lev2::PixelFetchContext::EPU_FLOAT;

  QInputEvent* qip = (QInputEvent*)EV->mpBlindEventData;

  bool bisshift = EV->mbSHIFT;

  switch (filtev._eventcode) {
    case ui::EventCode::KEY: {
      int ikeyc = filtev.miKeyCode;
      printf("ikeyc<%d>\n", ikeyc);

      switch (ikeyc) {
        case 'a': {
          if (false == mOutlinerModel.AreArchsEnabled())
            mOutlinerModel.ToggleArchs();
          else if (mOutlinerModel.AreArchsEnabled() && false == mOutlinerModel.AreCompsEnabled())
            mOutlinerModel.ToggleComps();
          else if (mOutlinerModel.AreArchsEnabled() && mOutlinerModel.AreCompsEnabled()) {
            mOutlinerModel.ToggleArchs();
            mOutlinerModel.ToggleComps();
          }
          break;
        }
        case '!': {
          mDark = !mDark;
          SetDirty();
          break;
        }
        case 'e': {
          mOutlinerModel.ToggleEnts();
          break;
        }
        case 'f': {
          fvec3 new_target;

          auto scene             = ed.GetSceneData();
          const EntData* entdata = nullptr;
          // TODO: Implement Visitor pattern to collect and grow bounding boxes for selected items
          const auto& selection = ed.selectionManager().getActiveSelection();
          for (auto it : selection) {
            if (const EntData* an_entdata = rtti::autocast(it)) {
              entdata                  = an_entdata;
              const DagNode& dnode     = entdata->GetDagNode();
              const TransformNode& t3d = dnode.GetTransformNode();
              auto pos                 = t3d.GetTransform().GetPosition();
              new_target               = pos;
            }
          }

          //////////////////////
          // find the editor cameras
          //////////////////////

          auto edcams = scene->FindEntitiesOfArchetype<ent::EditorCamArchetype>();

          for (auto edc : edcams) {
            auto comp = edc->GetTypedComponent<EditorCamControllerData>();

            if (comp) {
              auto cam = (lev2::EzUiCam*)comp->GetCamera();
              // auto eye = cam->GetEye();
              // auto tgt = cam->GetTarget();
              // auto dir = (tgt-eye).Normal();
              // auto upd = cam->GetUp();

              cam->mvCenter = new_target;
              cam->updateMatrices();
            }
          }

          //////////////////////

          break;
        }
        case 'g': {
          mOutlinerModel.ToggleGlobals();
          break;
        }
        case 's': {
          mOutlinerModel.ToggleSystems();
          break;
        }
        case '\n': {
          SetNameOfSelectedItem();
          break;
        }
        case Qt::Key_Delete: { // delete
          int ilastsel = mOutlinerModel.GetLastSelection();
          if (ilastsel >= 0) {
            const std::vector<Outliner2Item>& items = mOutlinerModel.Items();
            const Outliner2Item& item               = items[ilastsel];
            if (item.mObject) {
              printf("OUTLINER2 delete obj<%p>\n", item.mObject);
              ed.EditorDeleteObject(item.mObject);
            }
          }
          break;
        }
        case Qt::Key_Up: // cursup
          mOutlinerModel.DecSel();
          break;
        case Qt::Key_Down: // cursdn
          mOutlinerModel.IncSel();
          break;
        default:
          break;
      }
      break;
    }
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE: {
      int idelta = EV->miMWY;

      // printf("Outliner2Surface<%p>::DoOnUiEvent EventCode::PUSH\n", this);

      GetPixel(ilocx, ilocy, ctx);
      float fx                   = float(ilocx) / float(width());
      float fy                   = float(ilocy) / float(height());
      ork::rtti::ICastable* pobj = ctx.GetObject(_pickbuffer, 0);

      bool is_in_set = true; // IsObjInSet(pobj);

      // orkprintf("Object<%p> is_in_set<%d> ilocx<%d> ilocy<%d> fx<%f> fy<%f>\n", pobj, int(is_in_set), ilocx, ilocy, fx, fy);
      mNeedsSurfaceRepaint = true;

      if (pobj) {
        sm.ClearSelection();
        sm.AddObjectToSelection((ork::Object*)pobj);
      }
      break;
    }
    case ui::EventCode::DOUBLECLICK: {
      SetNameOfSelectedItem();
      break;
    }
    case ui::EventCode::MOUSEWHEEL: {
      int idelta = EV->miMWY;
      miScrollY += idelta;

      int scrollb = -(mContentH - height());
      // printf("miScrollY<%d> mContentH<%d> scrollb<%d>\n", miScrollY, mContentH, scrollb);
      if (miScrollY < scrollb)
        miScrollY = scrollb;
      if (miScrollY > 0)
        miScrollY = 0;
      mNeedsSurfaceRepaint = true;

      break;
    }
    default:
      break;
  }
  return ret;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
