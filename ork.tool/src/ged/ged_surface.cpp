////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/properties/IProperty.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/math/basicfilters.h>
#include <ork/kernel/msgrouter.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////
fvec4 ged::GedSurface::AssignPickId(GedObject* pobj) {
  uint64_t pid = _pickbuffer->AssignPickId(pobj);
  fvec4 out;
  out.SetRGBAU64(pid);
  return out;
}
///////////////////////////////////////////////////////////////////////////////
namespace ged {
///////////////////////////////////////////////////////////////////////////////
static const int kscrollw = 32;
orkset<GedSurface*> GedSurface::gAllViewports;
///////////////////////////////////////////////////////////////////////////////
GedSurface::GedSurface(const std::string& name, ObjModel& model)
    : ui::Surface(name, 0, 0, 0, 0, fcolor3::Black(), 0.0f)
    , mModel(model)
    , mWidget(model)
    , mpActiveNode(nullptr)
    , miScrollY(0)
    , mpMouseOverNode(0) {
  mWidget.setViewport(this);

  gAllViewports.insert(this);

  object::Connect(&model.GetSigRepaint(), &mWidget.GetSlotRepaint());
  object::Connect(&model.GetSigModelInvalidated(), &mWidget.GetSlotModelInvalidated());

  _simulation_subscriber = msgrouter::channel("Simulation")->subscribe([=](msgrouter::content_t c) { this->onInvalidate(); });
}
GedSurface::~GedSurface() {
  orkset<GedSurface*>::iterator it = gAllViewports.find(this);

  if (it != gAllViewports.end()) {
    gAllViewports.erase(it);
  }

  if (_pickbuffer)
    delete _pickbuffer;
}

///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoInit(lev2::Context* pt) {
  auto par    = pt->FBI()->GetThisBuffer();
  _pickbuffer = new ork::lev2::PickBuffer(this, pt, 0, 0);
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoSurfaceResize() {
  mWidget.SetDims(width(), height());

  if (0 == _pickbuffer && (nullptr != _target)) {
    _pickbuffer->resize(width(), height());
  }
  // TODO: _pickbuffer->Resize()
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  // ork::tool::ged::ObjModel::FlushAllQueues();

  // orkprintf( "GedSurface::DoDraw()\n" );

  auto tgt = drwev->GetTarget();
  tgt->debugPushGroup(FormatString("GedSurface::repaint"));
  auto mtxi     = tgt->MTXI();
  auto fbi      = tgt->FBI();
  int pickstate = fbi->miPickState;
  // bool bispick = framedata().IsPickMode();

  // printf("GedSurface<%p>::Draw x<%d> y<%d> w<%d> h<%d> pickstate<%d>\n", this, miX, miY, width(), height(), pickstate);

  //////////////////////////////////////////////////
  // Compute Scoll Transform
  //////////////////////////////////////////////////

  ork::fmtx4 matSCROLL;
  matSCROLL.SetTranslation(0.0f, float(miScrollY), 0.0f);

  //////////////////////////////////////////////////

  fbi->pushScissor(ViewportRect(0, 0, width(), height()));
  fbi->pushViewport(ViewportRect(0, 0, width(), height()));
  mtxi->PushMMatrix(matSCROLL);
  {
    fbi->Clear(GetClearColorRef(), 1.0f);

    auto pobj = mModel.CurrentObject();
    if (pobj) {
      GedWidget* pw = mModel.GetGedWidget();

      pw->Draw(tgt, width(), height(), miScrollY);
    }
  }
  mtxi->PopMMatrix();
  fbi->popViewport();
  fbi->popScissor();
  tgt->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void GedSurface::onInvalidate() {
  mpActiveNode = nullptr;
  MarkSurfaceDirty();
}

ui::HandlerResult GedSurface::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);

  // printf("GedSurface<%p> uievent\n", this);

  const auto& filtev = EV->mFilteredEvent;

  int ix = EV->miX;
  int iy = EV->miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);

  lev2::PixelFetchContext ctx;
  ctx.miMrtMask = (1 << 0); //| (1 << 1); // ObjectID and ObjectUVD
  ctx.mUsage[0] = lev2::PixelFetchContext::EPU_PTR64;
  // ctx.mUsage[1] = lev2::PixelFetchContext::EPU_FLOAT;

  bool filt_kpush = (filtev.mAction == "keypush");

  bool filt_leftbutton   = filtev.mBut0;
  bool filt_middlebutton = filtev.mBut1;
  bool filt_rightbutton  = filtev.mBut2;

  auto qip = (QInputEvent*)EV->mpBlindEventData;

  bool bisshift = EV->mbSHIFT;

  auto locEV = std::make_shared<ui::Event>(*EV.get());

  locEV->miX    = ilocx;
  locEV->miY    = ilocy - miScrollY;
  locEV->miRawX = locEV->miX;
  locEV->miRawY = locEV->miY;

  if (mpActiveNode)
    mpActiveNode->OnUiEvent(locEV);

  switch (filtev._eventcode) {
    case ui::EventCode::KEY: {
      int mikeyc = filtev.miKeyCode;
      if (mikeyc == '!') {
        mWidget.IncrementSkin();
        mNeedsSurfaceRepaint = true;
      }
      break;
    }
    case ui::EventCode::MOUSEWHEEL: {
      int iscrollamt = bisshift ? 32 : 8;

      // if( pobj )
      {
        int idelta = EV->miMWY;

        if (idelta > 0) {
          miScrollY += iscrollamt;
          if (miScrollY > 0)
            miScrollY = 0;
        } else if (idelta < 0) {
          ////////////////////////////////////
          // iwh = 500
          // irh = 200
          // ism = 300 // 0
          ////////////////////////////////////

          ////////////////////////////////////
          // iwh = 300
          // irh = 500
          // ism = -200
          ////////////////////////////////////

          int iwh        = height();                // 500
          int irh        = mWidget.GetRootHeight(); // 200
          int iscrollmin = (iwh - irh);             // 300

          if (iscrollmin > 0) {
            iscrollmin = 0;
          }

          miScrollY -= iscrollamt;
          if (miScrollY < iscrollmin) {
            miScrollY = iscrollmin;
          }
          printf("predelta<%d> iscrollmin<%d> miScrollY<%d>\n", idelta, iscrollmin, miScrollY);
        }
      }

      mNeedsSurfaceRepaint = true;
      break;
    }
    case ui::EventCode::MOVE: {
      QMouseEvent* qem = (QMouseEvent*)qip;
      static int gctr  = 0;
      if (0 == gctr % 4) {
        GetPixel(ilocx, ilocy, ctx);
        rtti::ICastable* pobj = ctx.GetObject(_pickbuffer, 0);
        if (0) // TODO pobj )
        {
          GedObject* pnode = rtti::autocast(pobj);
          if (pnode) { // pnode->mouseMoveEvent( & myme );
            mpMouseOverNode = pnode;

            if (pnode != mpActiveNode)
              pnode->OnUiEvent(locEV);
          }
        }
      }
      gctr++;
      break;
    }
    case ui::EventCode::DRAG: {
      if (mpActiveNode) {
        if (GedItemNode* as_inode = ork::rtti::autocast(mpActiveNode)) {
          locEV->miX -= as_inode->GetX();
          locEV->miY -= as_inode->GetY();
        }
        mpActiveNode->OnUiEvent(locEV);
        mNeedsSurfaceRepaint = true;
      } else {
        // pnode->mouseMoveEvent( & myme );
      }
      break;
    }
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE:
    case ui::EventCode::DOUBLECLICK: {

      QMouseEvent* qem = (QMouseEvent*)qip;

      GetPixel(ilocx, ilocy, ctx);
      float fx                   = float(ilocx) / float(width());
      float fy                   = float(ilocy) / float(height());
      ork::rtti::ICastable* pobj = ctx.GetObject(_pickbuffer, 0);

      bool is_in_set = IsObjInSet(pobj);
      const auto clr = ctx._pickvalues[0];
      // printf("GetPixel color<%g %g %g %g>\n", clr.x, clr.y, clr.z, clr.w);
      /*printf(
          "GedSurface<%p> Object<%p> is_in_set<%d> ilocx<%d> ilocy<%d> fx<%f> fy<%f>\n", //
          this,
          pobj,
          int(is_in_set),
          ilocx,
          ilocy,
          fx,
          fy);*/

      /////////////////////////////////////
      // test object against known set
      if (false == is_in_set)
        pobj = 0;
      /////////////////////////////////////

      if (GedObject* pnode = ork::rtti::autocast(pobj)) {
        if (GedItemNode* as_inode = ork::rtti::autocast(pobj)) {
          locEV->miX -= as_inode->GetX();
          locEV->miY -= as_inode->GetY();
        }

        switch (filtev._eventcode) {
          case ui::EventCode::PUSH:
            mpActiveNode = pnode;
            if (pnode)
              pnode->OnUiEvent(locEV);
            break;
          case ui::EventCode::RELEASE:
            if (pnode)
              pnode->OnUiEvent(locEV);
            mpActiveNode = nullptr;
            break;
          case ui::EventCode::DOUBLECLICK:
            if (pnode)
              pnode->OnUiEvent(locEV);
            break;
        }
      }

      mNeedsSurfaceRepaint = true;
      break;
    }
    default:
      break;
  }
  return ret;
}
///////////////////////////////////////////////////////////////////////////////////
static std::set<void*>& GetObjSet() {
  static std::set<void*> gObjSet;
  return gObjSet;
}
void ClearObjSet() {
  GetObjSet().clear();
}
void AddToObjSet(void* pobj) {
  GetObjSet().insert(pobj);
}
bool IsObjInSet(void* pobj) {
  bool rval = false;
  rval      = (GetObjSet().find(pobj) != GetObjSet().end());
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ged
///////////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
