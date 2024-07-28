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
#include <ork/kernel/environment.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
fvec4 ged::GedSurface::AssignPickId(GedObject* pobj) {
  if (_pickbuffer) {
    uint64_t pid = _pickbuffer->AssignPickId(pobj);
    fvec4 out;
    out.setRGBAU64(pid);
    return out;
  }
  return fvec4(0, 0, 0, 0);
}
///////////////////////////////////////////////////////////////////////////////
static const int kscrollw = 8;
orkset<GedSurface*> GedSurface::gAllViewports;
///////////////////////////////////////////////////////////////////////////////
GedSurface::GedSurface(const std::string& name, objectmodel_ptr_t model)
    : ui::Surface(name, 0, 0, 0, 0, fcolor3::Black(), 0.0f)
    , _model(model)
    , _container(model)
    , miScrollY(0){

  _container._viewport = this;

  gAllViewports.insert(this);

  _connection_repaint = _model->_sigRepaint.connect([this]() { this->MarkSurfaceDirty(); }); // Connect the signal to the slot

  // object::Connect(&model.GetSigRepaint(), &_container.GetSlotRepaint());
  // object::Connect(&model.GetSigModelInvalidated(), &_container.GetSlotModelInvalidated());

  _simulation_subscriber = msgrouter::channel("Simulation")->subscribe([=](msgrouter::content_t c) { this->onInvalidate(); });
}
///////////////////////////////////////////////////////////////////////////////
GedSurface::~GedSurface() {

  _connection_repaint.disconnect();

  orkset<GedSurface*>::iterator it = gAllViewports.find(this);

  if (it != gAllViewports.end()) {
    gAllViewports.erase(it);
  }

  if (_pickbuffer)
    delete _pickbuffer;
}

///////////////////////////////////////////////////////////////////////////////
void GedSurface::_doGpuInit(lev2::Context* pt) {
  Surface::_doGpuInit(pt);
  _container.gpuInit(pt);
  auto par    = pt->FBI()->GetThisBuffer();
  _pickbuffer = new ork::lev2::PickBuffer(this, pt, 0, 0);
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoSurfaceResize() {
  _container.SetDims(width(), height());
}
///////////////////////////////////////////////////////////////////////////////
void GedSurface::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  auto context = drwev->GetTarget();
  context->debugPushGroup(FormatString("GedSurface::repaint"));
  auto mtxi = context->MTXI();
  auto fbi  = context->FBI();
  auto dwi  = context->DWI();

  int orig_pickstate = fbi->_pickState;
  //fbi->_pickState = true;
  int pickstate = fbi->_pickState;

  int W = width();
  int H = height();

  //////////////////////////////////////////////////

  fbi->pushScissor(ViewportRect(0, 0, W, H));
  fbi->pushViewport(ViewportRect(0, 0, W, H));
  {

    if (pickstate == 0) {
      fbi->Clear(fvec4(0, 0, 0, 0), 1.0f);
    } else {
      fbi->Clear(fvec4(0, 0, 0.5, 0), 1.0f);
      // printf( "GedSurface::repaint pickstate<%d> W<%d> H<%d>\n", pickstate, W, H );
    }

    if (_model->_currentObject) {
      // printf("miScrollY<%d>\n", miScrollY);
      _container.Draw(context, W, H, miScrollY);
    }
  }
  fbi->popViewport();
  fbi->popScissor();
  context->debugPopGroup();

  fbi->_pickState = orig_pickstate;
}

///////////////////////////////////////////////////////////////////////////////

int GedSurface::_clampedScroll(int scroll) const {
  int iwh        = height();                   // 500
  int irh        = _container.GetRootHeight(); // 200
  int iscrollmin = (iwh - irh);                // 300
  if (iscrollmin > 0) {
    iscrollmin = 0;
  }
  if (scroll < iscrollmin) {
    scroll = iscrollmin;
  }
  if (scroll > 0)
    scroll = 0;
  return scroll;
}

///////////////////////////////////////////////////////////////////////////////

void GedSurface::onInvalidate() {
  _activeNode = nullptr;
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

  lev2::PixelFetchContext ctx(1);
  ctx.miMrtMask = (1 << 0); //| (1 << 1); // ObjectID and ObjectUVD
  ctx._usage[0] = lev2::PixelFetchContext::EPU_PTR64;

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
  locEV->miScreenPosX = EV->miScreenPosX;
  locEV->miScreenPosY = EV->miScreenPosY;

  if (_activeNode){
   // bool was_handled = _activeNode->OnUiEvent(locEV);
  }

  switch (filtev._eventcode) {
    case ui::EventCode::KEY_DOWN:
    case ui::EventCode::KEY_REPEAT: {
      int mikeyc = filtev.miKeyCode;
      printf("key<%d>\n", mikeyc);
      switch (mikeyc) {
        case 'L': { // load
          if( EV->mbCTRL ){
            std::string default_path;
            genviron.get("ORKID_WORKSPACE_DIR",default_path);
            auto P = file::Path(default_path)/"ORKFILE.orj";
            auto path = ui::popupOpenDialog( //
                "Open Orkid Json Object File", //
                P.c_str(), //
                {"*.orj"}, //
                false); //
            ork::File inputfile(path, ork::EFM_READ);
            size_t length = 0;
            inputfile.GetLength(length);
            auto dblock = std::make_shared<DataBlock>();
            auto dest  = (char*) dblock->allocateBlock(length+1);
            inputfile.Read((void*)dest, length);
            inputfile.Close();
            dest[length] = 0; // null terminate
            object_ptr_t instance_out;
            reflect::serdes::JsonDeserializer deser(dest);
            deser.deserializeTop(instance_out);
          }
          break;
        }
        case 'S': { // save
          if( EV->mbCTRL ){
            auto obj = _model->_currentObject;
            if(obj){
              std::string default_path;
              genviron.get("ORKID_WORKSPACE_DIR",default_path);
              auto P = file::Path(default_path)/"ORKFILE.orj";
              auto path = ui::popupSaveDialog( //
                  "Save Orkid Json Object File", //
                  P.c_str(), //
                  {"*.orj"}); //
              printf( "path<%s>\n", path.c_str() );
              reflect::serdes::JsonSerializer ser;
              auto topnode    = ser.serializeRoot(obj);
              auto resultdata = ser.output();
              ork::File outputfile(path, ork::EFM_WRITE);
              outputfile.Write(resultdata.c_str(), resultdata.length());
              outputfile.Close();
            }
          }
          break;
        }
        case 264: { // CURS UP
          miScrollY            = _clampedScroll(miScrollY - 16);
          mNeedsSurfaceRepaint = true;
          break;
        }
        case 265: { // CURS DOWN
          miScrollY            = _clampedScroll(miScrollY + 16);
          mNeedsSurfaceRepaint = true;
          break;
        }
        case 266: { // PG DOWN
          miScrollY            = _clampedScroll(miScrollY + 128);
          mNeedsSurfaceRepaint = true;
          break;
        }
        case 267: { // PG UP
          miScrollY            = _clampedScroll(miScrollY - 128);
          mNeedsSurfaceRepaint = true;
          break;
        }
        case 268: { // HOME
          miScrollY            = 0;
          mNeedsSurfaceRepaint = true;
          break;
        }
        case 269: { // END
          miScrollY            = _clampedScroll(-100000);
          mNeedsSurfaceRepaint = true;
          break;
        }
        case '!': {
          _container.IncrementSkin();
          mNeedsSurfaceRepaint = true;
        }
        default:
          break;
      }
      break;
    }
    case ui::EventCode::MOUSEWHEEL: {
      int iscrollamt = bisshift ? 32 : 8;

      // if( pobj )
      {
        int idelta = EV->miMWY;

        if (idelta > 0) {
          miScrollY = _clampedScroll(miScrollY + iscrollamt);
        } else if (idelta < 0) {
          miScrollY = _clampedScroll(miScrollY - iscrollamt);
          // printf("predelta<%d> iscrollmin<%d> miScrollY<%d>\n", idelta, iscrollmin, miScrollY);
        }
      }

      mNeedsSurfaceRepaint = true;
      break;
    }
    case ui::EventCode::MOVE: {
      static int gctr = 0;

      if (0 == gctr % 4) {
        GetPixel(ilocx, ilocy, ctx);
        auto pobj = (ork::Object*)ctx.GetObject(_pickbuffer, 0);
        if(0)printf( "move ilocx<%d> ilocy<%d> pobj<%p> ", ilocx, ilocy, (void*) pobj );
        if( pobj ) {
            if(0)printf( "clazz<%s> ", pobj->objectClass()->Name().c_str() );
          auto pnode = dynamic_cast<GedObject*>(pobj);
          if (pnode) {
            if(0)printf( "pnode<%p> ", (void*) pnode );
            auto as_inode = dynamic_cast<GedItemNode*>(pobj);
            if( as_inode ){
              if(0)printf( "as_inode<%s>", as_inode->_propname.c_str() );
            }
            _mouseoverNode = pnode;
            if (pnode != _activeNode){
              bool was_handled = pnode->OnUiEvent(locEV);
            }
          }
        }
        if(0)printf( "\n");
        mNeedsSurfaceRepaint = true;
      }
      gctr++;
      break;
    }
    case ui::EventCode::DRAG: {
      if (_activeNode) {
        auto as_item_node = dynamic_cast<GedItemNode*>(_activeNode);
        if (as_item_node) {
          locEV->miX -= as_item_node->GetX();
          locEV->miY -= as_item_node->GetY();
        }
        bool was_handled = _activeNode->OnUiEvent(locEV);
        mNeedsSurfaceRepaint = true;
      }
      break;
    }
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE:
    case ui::EventCode::DOUBLECLICK: {

      GetPixel(ilocx, ilocy, ctx);
      float fx  = float(ilocx) / float(width());
      float fy  = float(ilocy) / float(height());
      auto pobj = ctx.GetObject(_pickbuffer, 0);

      printf( "GedSurface:: pick ilocx<%d> ilocy<%d> fx<%g> fy<%g> pobj<%p>\n", ilocx, ilocy, fx, fy, (void*) pobj );

      bool is_in_set = GedSkin::IsObjInSet(pobj);
      const auto clr = ctx._pickvalues[0];
      /////////////////////////////////////
      // test object against known set
      if (false == is_in_set)
        pobj = 0;
      /////////////////////////////////////

      if (auto pnode = dynamic_cast<GedObject*>(pobj)) {
        if (auto as_inode = dynamic_cast<GedItemNode*>(pobj)) {
          locEV->miX -= as_inode->GetX();
          locEV->miY -= as_inode->GetY();
          auto clazz     = as_inode->GetClass();
          auto clazzname = clazz->Name();
           printf( "obj<%p> class<%s>\n", (void*) pobj, clazzname.c_str() );
        }

        switch (filtev._eventcode) {
          case ui::EventCode::PUSH:{
            _activeNode = pnode;
            bool was_handled = pnode->OnUiEvent(locEV);
            break;
          }
          case ui::EventCode::RELEASE:{
            bool was_handled = pnode->OnUiEvent(locEV);
            _activeNode = nullptr;
            break;
          }
          case ui::EventCode::DOUBLECLICK:{
            _activeNode = pnode;
            bool was_handled = pnode->OnUiEvent(locEV);
            break;
          }
          default:
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
void GedSurface::ResetScroll() {
  miScrollY = 0;
}
const GedObject* GedSurface::GetMouseOverNode() const {
  return _mouseoverNode;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
///////////////////////////////////////////////////////////////////////////////
