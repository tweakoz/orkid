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
#include <ork/lev2/gfx/rtgroup.h>
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
  _alwaysRepaint = true;
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
      //fbi->Clear(fvec4(0, 0, 0, 0), 1.0f);
    } else {
      //fbi->Clear(fvec4(0, 0, 0.5, 0), 1.0f);
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

///////////////////////////////////////////////////////////////////////////////
// Event handler helper methods
///////////////////////////////////////////////////////////////////////////////

void GedSurface::_onUiEventDoLoad(ui::event_constptr_t EV) {
  if (!EV->mbCTRL) return;
  
  std::string default_path;
  genviron.get("ORKID_WORKSPACE_DIR", default_path);
  auto P = file::Path(default_path) / "ORKFILE.orj";
  auto path = ui::popupOpenDialog(
      "Open Orkid Json Object File",
      P.c_str(),
      {"*.orj"},
      false);
  
  ork::File inputfile(path, ork::EFM_READ);
  size_t length = 0;
  inputfile.GetLength(length);
  auto dblock = std::make_shared<DataBlock>();
  auto dest = (char*)dblock->allocateBlock(length + 1);
  inputfile.Read((void*)dest, length);
  inputfile.Close();
  dest[length] = 0; // null terminate
  
  object_ptr_t instance_out;
  reflect::serdes::JsonDeserializer deser(dest);
  deser.deserializeTop(instance_out);
}

void GedSurface::_onUiEventDoSave(ui::event_constptr_t EV) {
  if (!EV->mbCTRL) return;
  
  auto obj = _model->_currentObject;
  if (!obj) return;
  
  std::string default_path;
  genviron.get("ORKID_WORKSPACE_DIR", default_path);
  auto P = file::Path(default_path) / "ORKFILE.orj";
  auto path = ui::popupSaveDialog(
      "Save Orkid Json Object File",
      P.c_str(),
      {"*.orj"});
  
  printf("path<%s>\n", path.c_str());
  reflect::serdes::JsonSerializer ser;
  auto topnode = ser.serializeRoot(obj);
  auto resultdata = ser.output();
  ork::File outputfile(path, ork::EFM_WRITE);
  outputfile.Write(resultdata.c_str(), resultdata.length());
  outputfile.Close();
}

void GedSurface::_onUiEventDoKeyboard(ui::event_constptr_t EV) {
  int mikeyc = EV->mFilteredEvent.miKeyCode;
  printf("key<%d>\n", mikeyc);
  
  switch (mikeyc) {
    case 'L': // load
      _onUiEventDoLoad(EV);
      break;
    case 'S': // save
      _onUiEventDoSave(EV);
      break;
    case 264: // CURS UP
      miScrollY = _clampedScroll(miScrollY - 16);
      mNeedsSurfaceRepaint = true;
      break;
    case 265: // CURS DOWN
      miScrollY = _clampedScroll(miScrollY + 16);
      mNeedsSurfaceRepaint = true;
      break;
    case 266: // PG DOWN
      miScrollY = _clampedScroll(miScrollY + 128);
      mNeedsSurfaceRepaint = true;
      break;
    case 267: // PG UP
      miScrollY = _clampedScroll(miScrollY - 128);
      mNeedsSurfaceRepaint = true;
      break;
    case 268: // HOME
      miScrollY = 0;
      mNeedsSurfaceRepaint = true;
      break;
    case 269: // END
      miScrollY = _clampedScroll(-100000);
      mNeedsSurfaceRepaint = true;
      break;
    case '!':
      _container.IncrementSkin();
      mNeedsSurfaceRepaint = true;
      break;
    default:
      break;
  }
}

void GedSurface::_onUiEventDoMouseWheel(ui::event_constptr_t EV) {
  bool bisshift = EV->mbSHIFT;
  int iscrollamt = bisshift ? 32 : 8;
  int idelta = EV->miMWY;
  
  if (idelta > 0) {
    miScrollY = _clampedScroll(miScrollY + iscrollamt);
  } else if (idelta < 0) {
    miScrollY = _clampedScroll(miScrollY - iscrollamt);
  }
  
  mNeedsSurfaceRepaint = true;
}

void GedSurface::_onUiEventDoMove(ui::event_constptr_t EV, ui::event_ptr_t locEV) {
  static int gctr = 0;
  
  if (0 != gctr % 4) {
    gctr++;
    return;
  }
  gctr++;
  
  int ilocx, ilocy;
  RootToLocal(EV->miX, EV->miY, ilocx, ilocy);
  
  // Prepare pixel fetch context for picking
  lev2::PixelFetchContext pfc(1);
  pfc.miMrtMask = (1 << 0);
  pfc._usage[0] = lev2::PixelFetchContext::EPixelUsage::PTR64;
  
  // Trigger pickbuffer rendering for mouseover detection
  if (_pickbuffer) {
    auto tgt = _pickbuffer->context();
    auto fbi = tgt->FBI();
    pfc._rtgroup = _pickbuffer->_rtgroup;
    
    // Render the pick buffer (preserving original rendering code)
    fbi->pushViewport(0, 0, width(), height());
    fbi->pushScissor(0, 0, width(), height());
    _pickbuffer->Draw(pfc);
    fbi->popViewport();
    fbi->popScissor();
    
    // TODO: Implement async capture for mouseover
    // For now, we can't get the object under the mouse
    // auto pobj = (ork::Object*)ctx.GetObject(_pickbuffer, 0);
    
    if(0)printf( "move ilocx<%d> ilocy<%d> (async capture TODO)\n", ilocx, ilocy);
    
    // The original code would set _mouseoverNode here
    // and call OnUiEvent if it changed from _activeNode
  }
  
  mNeedsSurfaceRepaint = true;
}

void GedSurface::_onUiEventDoDrag(ui::event_constptr_t EV, ui::event_ptr_t locEV) {
  if (!_activeNode) return;
  
  auto as_item_node = dynamic_cast<GedItemNode*>(_activeNode);
  if (as_item_node) {
    locEV->miX -= as_item_node->GetX();
    locEV->miY -= as_item_node->GetY();
  }
  bool was_handled = _activeNode->OnUiEvent(locEV);
  mNeedsSurfaceRepaint = true;
}

void GedSurface::_onUiEventDoMouseButton(ui::event_constptr_t EV, ui::event_ptr_t locEV) {
  int ilocx, ilocy;
  RootToLocal(EV->miX, EV->miY, ilocx, ilocy);
  
  // Prepare pixel fetch context for picking
  lev2::PixelFetchContext pfc(1);
  pfc.miMrtMask = (1 << 0);
  pfc._usage[0] = lev2::PixelFetchContext::EPixelUsage::PTR64;
  
  float fx = float(ilocx) / float(width());
  float fy = float(ilocy) / float(height());
  
  // Trigger pickbuffer rendering and capture asynchronously
  if (_pickbuffer) {
    auto tgt = _pickbuffer->context();
    auto fbi = tgt->FBI();
    pfc._rtgroup = _pickbuffer->_rtgroup;
    
    // Render the pick buffer (preserving original rendering code)
    fbi->pushViewport(0, 0, width(), height());
    fbi->pushScissor(0, 0, width(), height());
    _pickbuffer->Draw(pfc);
    fbi->popViewport();
    fbi->popScissor();
    
    // Now instead of GetPixel, we need to capture asynchronously
    // For now, just handle the event without the pick result
    printf("GedSurface:: pick ilocx<%d> ilocy<%d> fx<%g> fy<%g> (async capture TODO)\n", ilocx, ilocy, fx, fy);
    
    // TODO: Implement async capture of the rendered pickbuffer
    // auto rtb = _pickbuffer->_rtgroup->buffer(0).get();
    // auto future = fbi->captureAsFormat(rtb, capbuf, lev2::EBufferFormat::RGBA8);
    
    switch (EV->mFilteredEvent._eventcode) {
      case ui::EventCode::PUSH:
        // Will set _activeNode based on async pick result
        break;
      case ui::EventCode::RELEASE:
        _activeNode = nullptr;
        break;
      case ui::EventCode::DOUBLECLICK:
        // Will set _activeNode based on async pick result
        break;
      default:
        break;
    }
  } else {
    // No pickbuffer, just handle the event
    switch (EV->mFilteredEvent._eventcode) {
      case ui::EventCode::PUSH:
        break;
      case ui::EventCode::RELEASE:
        _activeNode = nullptr;
        break;
      case ui::EventCode::DOUBLECLICK:
        break;
      default:
        break;
    }
  }
  
  mNeedsSurfaceRepaint = true;
}

///////////////////////////////////////////////////////////////////////////////
// Main event handler
///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult GedSurface::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);
  
  const auto& filtev = EV->mFilteredEvent;
  
  int ix = EV->miX;
  int iy = EV->miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);
  
  auto locEV = std::make_shared<ui::Event>(*EV.get());
  locEV->miX = ilocx;
  locEV->miY = ilocy - miScrollY;
  locEV->miRawX = locEV->miX;
  locEV->miRawY = locEV->miY;
  locEV->miScreenPosX = EV->miScreenPosX;
  locEV->miScreenPosY = EV->miScreenPosY;
  
  switch (filtev._eventcode) {
    case ui::EventCode::KEY_DOWN:
    case ui::EventCode::KEY_REPEAT:
      _onUiEventDoKeyboard(EV);
      break;
      
    case ui::EventCode::MOUSEWHEEL:
      _onUiEventDoMouseWheel(EV);
      break;
      
    case ui::EventCode::MOVE:
      _onUiEventDoMove(EV, locEV);
      break;
      
    case ui::EventCode::DRAG:
      _onUiEventDoDrag(EV, locEV);
      break;
      
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE:
    case ui::EventCode::DOUBLECLICK:
      _onUiEventDoMouseButton(EV, locEV);
      break;
      
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

void GedSurface::_processCapturedPick(int ilocx, int ilocy, float fx, float fy) {
  // TODO: Implement async pick processing when infrastructure is ready
  // This will decode the pick ID from the captured pixel data
  // and handle the appropriate UI event
  
  printf("GedSurface:: _processCapturedPick called (not yet implemented)\n");
  
  // Clear pending state
  _pendingPickFuture = nullptr;
  _pendingPickEvent = nullptr;
  _pendingPickLocEvent = nullptr;
  
  mNeedsSurfaceRepaint = true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
///////////////////////////////////////////////////////////////////////////////
