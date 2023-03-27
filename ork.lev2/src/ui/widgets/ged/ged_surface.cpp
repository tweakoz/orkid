////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/math/misc_math.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
fvec4 ged::GedSurface::AssignPickId(GedObject* pobj) {
  uint64_t pid = _pickbuffer->AssignPickId(pobj);
  fvec4 out;
  out.setRGBAU64(pid);
  return out;
}
///////////////////////////////////////////////////////////////////////////////
static const int kscrollw = 32;
orkset<GedSurface*> GedSurface::gAllViewports;
///////////////////////////////////////////////////////////////////////////////
GedSurface::GedSurface(const std::string& name, objectmodel_ptr_t model)
    : ui::Surface(name, 0, 0, 0, 0, fcolor3::Black(), 0.0f)
    , _model(model)
    , _container(model)
    , mpActiveNode(nullptr)
    , miScrollY(0)
    , mpMouseOverNode(0) {

//  _container.setViewport(this);

  gAllViewports.insert(this);

  //object::Connect(&model.GetSigRepaint(), &_container.GetSlotRepaint());
  //object::Connect(&model.GetSigModelInvalidated(), &_container.GetSlotModelInvalidated());

  _simulation_subscriber = msgrouter::channel("Simulation")->subscribe([=](msgrouter::content_t c) { this->onInvalidate(); });
}
///////////////////////////////////////////////////////////////////////////////
GedSurface::~GedSurface() {
  orkset<GedSurface*>::iterator it = gAllViewports.find(this);

  if (it != gAllViewports.end()) {
    gAllViewports.erase(it);
  }

  if (_pickbuffer)
    delete _pickbuffer;
}

///////////////////////////////////////////////////////////////////////////////
void GedSurface::_doGpuInit(lev2::Context* pt) {
  auto par    = pt->FBI()->GetThisBuffer();
  _pickbuffer = new ork::lev2::PickBuffer(this, pt, 0, 0);
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoSurfaceResize() {
  _container.SetDims(width(), height());

  if (0 == _pickbuffer && (nullptr != _target)) {
    _pickbuffer->resize(width(), height());
  }
  // TODO: _pickbuffer->Resize()
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  auto tgt = drwev->GetTarget();
  tgt->debugPushGroup(FormatString("GedSurface::repaint"));
  auto mtxi     = tgt->MTXI();
  auto fbi      = tgt->FBI();
  int pickstate = fbi->_pickState;

  //////////////////////////////////////////////////
  // Compute Scoll Transform
  //////////////////////////////////////////////////

  ork::fmtx4 matSCROLL;
  matSCROLL.setTranslation(0.0f, float(miScrollY), 0.0f);

  //////////////////////////////////////////////////

  fbi->pushScissor(ViewportRect(0, 0, width(), height()));
  fbi->pushViewport(ViewportRect(0, 0, width(), height()));
  mtxi->PushMMatrix(matSCROLL);
  {
    fbi->Clear(GetClearColorRef(), 1.0f);

    if (_model->_currentObject) {
      _container.Draw(tgt, width(), height(), miScrollY);
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

///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult GedSurface::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);

  const auto& filtev = EV->mFilteredEvent;

  int ix = EV->miX;
  int iy = EV->miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);

  lev2::PixelFetchContext ctx;
  ctx.miMrtMask = (1 << 0); //| (1 << 1); // ObjectID and ObjectUVD
  ctx.mUsage[0] = lev2::PixelFetchContext::EPU_PTR64;

  bool filt_kpush = (filtev.mAction == "keypush");

  bool filt_leftbutton   = filtev.mBut0;
  bool filt_middlebutton = filtev.mBut1;
  bool filt_rightbutton  = filtev.mBut2;

  bool bisshift = EV->mbSHIFT;

  auto locEV = std::make_shared<ui::Event>(*EV.get());

  locEV->miX    = ilocx;
  locEV->miY    = ilocy - miScrollY;
  locEV->miRawX = locEV->miX;
  locEV->miRawY = locEV->miY;

  if (mpActiveNode)
    mpActiveNode->OnUiEvent(locEV);

  switch (filtev._eventcode) {
    case ui::EventCode::KEY_DOWN: {
      int mikeyc = filtev.miKeyCode;
      if (mikeyc == '!') {
        _container.IncrementSkin();
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

          int iwh        = height();                // 500
          int irh        = _container.GetRootHeight(); // 200
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
      static int gctr  = 0;
      
      if (0 == gctr % 4) {
        GetPixel(ilocx, ilocy, ctx);
        auto pobj = (GedObject*) ctx.GetObject(_pickbuffer, 0);
        if (0) // TODO pobj )
        {
          auto pnode = dynamic_cast<GedObject*>(pobj);
          if (pnode) { 
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
        auto as_item_node = dynamic_cast<GedItemNode*>(mpActiveNode);
        if (as_item_node) {
          locEV->miX -= as_item_node->GetX();
          locEV->miY -= as_item_node->GetY();
        }
        mpActiveNode->OnUiEvent(locEV);
        mNeedsSurfaceRepaint = true;
      } else {
      }
      break;
    }
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE:
    case ui::EventCode::DOUBLECLICK: {

      /*QMouseEvent* qem = (QMouseEvent*)qip;

      GetPixel(ilocx, ilocy, ctx);
      float fx                   = float(ilocx) / float(width());
      float fy                   = float(ilocy) / float(height());
      ork::rtti::ICastable* pobj = ctx.GetObject(_pickbuffer, 0);

      bool is_in_set = IsObjInSet(pobj);
      const auto clr = ctx._pickvalues[0];
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
      }*/

      mNeedsSurfaceRepaint = true;
      break;
    }
    default:
      break;
  }
  return ret;
}
void GedSurface::ResetScroll() {
miScrollY = 0;
}
  const GedObject* GedSurface::GetMouseOverNode() const {
    return mpMouseOverNode;
  }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
///////////////////////////////////////////////////////////////////////////////