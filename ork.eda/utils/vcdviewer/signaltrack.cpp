#include "vcdviewer.h"
#include <ork/lev2/input/inputdevice.h>

///////////////////////////////////////////////////////////////////////////////

SignalTrackWidget::SignalTrackWidget(
    signal_ptr_t sig, //
    fvec4 color)
    : Widget("SignalTrackWidget")
    , _color(color)
    , _signal(sig)
    , _vtxbuf(1 << 20, 0, PrimitiveType::NONE) {
  _textcolor = fvec4(1, 1, 1, 1);
}

///////////////////////////////////////////////////////////////////////////////

void SignalTrackWidget::setTimeStamp(uint64_t timestep) {
  auto overlay                = Overlay::instance();
  auto viewparams             = ViewParams::instance();
  viewparams->_cursor_actual  = timestep;
  viewparams->_curtrack       = this;
  uint64_t closest            = nearestItem(_signal->_samples, timestep)->first;
  viewparams->_cursor_nearest = closest;
  ////////////////////////////////
  // update all tracks
  ////////////////////////////////
  auto font = lev2::FontMan::GetRef()._pushFont(_font);
  for (auto track : viewparams->_sigtracks) {

    auto sig = track->_signal;

    closest                  = nearestItem(sig->_samples, timestep)->first;
    track->_nearest_timestep = closest;
    auto sample              = sig->_samples[closest];
    bool is_1bit             = sig->_bit_width == 1;
    if (is_1bit) {
      track->_label       = "";
      track->_stringwidth = 0;
    } else {
      track->_label = sample->strvalue();
      int lablen    = track->_label.length();

      track->_stringwidth = lev2::FontMan::stringWidth(lablen);
    }
  }
  lev2::FontMan::PopFont();
  ////////////////////////////////

  overlay->_vbdirty = true;
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult SignalTrackWidget::DoOnUiEvent(event_constptr_t evptr) {
  auto overlay    = Overlay::instance();
  auto viewparams = ViewParams::instance();
  auto uictx      = evptr->_uicontext;

  switch (evptr->_eventcode) {
    case EventCode::KEY:
    case EventCode::KEY_REPEAT:
      switch (evptr->miKeyCode) {
        case ETRIG_RAW_KEY_LEFT: {
          auto it = nearestItem(_signal->_samples, _nearest_timestep);
          if (it != _signal->_samples.end()) {
            it--;
            if (it != _signal->_samples.end()) {
              setTimeStamp(it->first);
            }
          }
          break;
        }
        case ETRIG_RAW_KEY_RIGHT: {
          auto it = nearestItem(_signal->_samples, _nearest_timestep);
          if (it != _signal->_samples.end()) {
            it++;
            if (it != _signal->_samples.end()) {
              setTimeStamp(it->first);
            }
          }
          break;
        }
        default:
          break;
      }
      break;
    case EventCode::MOUSE_ENTER:
      printf("enter trakwidg<%p>\n", this);
      uictx->_overlayWidget = overlay.get();
      break;
    case EventCode::MOUSE_LEAVE:
      uictx->_overlayWidget = nullptr;
      break;
    case EventCode::MOVE: {
      int local_x = 0;
      int local_y = 0;
      RootToLocal(evptr->miX, evptr->miY, local_x, local_y);
      float ftimemin    = float(viewparams->_min_timestamp);
      float ftimemax    = float(viewparams->_max_timestamp);
      float ftimerange  = ftimemax - ftimemin;
      float fi          = float(local_x - 1) / float(_geometry._w - 2);
      fi                = std::clamp(fi, 0.0f, 1.0f);
      uint64_t timestep = uint64_t(ftimemin + (fi * ftimerange));
      setTimeStamp(timestep);
      break;
    }
    default:
      break;
  };
  return HandlerResult();
}

///////////////////////////////////////////////////////////////////////////////

void SignalTrackWidget::DoDraw(ui::drawevent_constptr_t drwev) {
  auto overlay    = Overlay::instance();
  auto viewparams = ViewParams::instance();
  _hdrlabel       = FormatString(
      "%s[%d] numpts<%zu>", //
      _signal->_longname.c_str(),
      _signal->_bit_width,
      _signal->_samples.size());

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto gbi    = tgt->GBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  bool is_1bit = _signal->_bit_width == 1;

  ////////////////////////////////
  if (_vbdirty) {
    _numsamples = _signal->_samples.size();
    _vbdirty    = false;
    VtxWriter<vtx_t> vw;
    vw.Lock(
        tgt, //
        &_vtxbuf,
        1 << 20);
    _numvertices = 0;
    auto add_vtx = [&](uint64_t timestamp, float value, uint32_t color) {
      float fx = float(timestamp);
      vtx_t vtx;
      vtx._position = fvec3(fx, value, 0.0f);
      vtx._color    = color;
      vw.AddVertex(vtx);
      _numvertices++;
    };

    uint64_t prevtimest     = _signal->_samples.begin()->first;
    sample_ptr_t prevsample = _signal->_samples.begin()->second;

    for (auto sitem : _signal->_samples) {

      uint64_t nexttimest     = sitem.first;
      sample_ptr_t nextsample = sitem.second;

      if (is_1bit) {
        float prevvalue = prevsample->read(0) ? 0.1 : 0.9;
        float nextvalue = nextsample->read(0) ? 0.1 : 0.9;
        add_vtx(prevtimest, prevvalue, 0xff00c000);
        add_vtx(nexttimest, prevvalue, 0xff00c000);
        add_vtx(nexttimest, prevvalue, 0xff00c000);
        add_vtx(nexttimest, nextvalue, 0xff00c000);
      } else {
        add_vtx(sitem.first, 0.0f, 0xff404040);
        add_vtx(sitem.first, 1.0f, 0xff404040);
      }
      prevtimest = sitem.first;
      prevsample = sitem.second;
    }
    //////////////////////////////////////////////
    // extend signal trace to end of session
    //////////////////////////////////////////////
    if (is_1bit) {
      auto enditem           = _signal->_samples.rbegin();
      uint64_t endtimest     = enditem->first;
      sample_ptr_t endsample = enditem->second;
      float endvalue         = endsample->read(0) ? 0.1 : 0.9;
      add_vtx(endtimest, endvalue, 0xff00ff00);
      add_vtx(viewparams->_max_timestamp, endvalue, 0xff00ff00);
    }
    //////////////////////////////////////////////
    vw.UnLock(tgt);
  }
  ////////////////////////////////
  float ftimemin       = float(viewparams->_min_timestamp);
  float ftimemax       = float(viewparams->_max_timestamp);
  float ftimerange     = ftimemax - ftimemin;
  float matrix_xscale  = float(_geometry._w) / ftimerange;
  float matrix_xoffset = float(_geometry._x) + 4;
  float matrix_yscale  = float(_geometry._h - 4);
  float matrix_yoffset = float(_geometry._y) + 5;
  fmtx4 scale_matrix, trans_matrix;
  scale_matrix.SetScale(matrix_xscale, matrix_yscale, 1.0f);
  trans_matrix.Translate(matrix_xoffset, matrix_yoffset, 0.0f);
  auto mmatrix = scale_matrix * trans_matrix;
  ////////////////////////////////
  int ix1, iy1, ix2, iy2, ixc, iyc;
  LocalToRoot(0, 0, ix1, iy1);
  ix2     = ix1 + _geometry._w;
  iy2     = iy1 + _geometry._h;
  ixc     = ix1 + (_geometry._w >> 1);
  iyc     = iy1 + (_geometry._h >> 1);
  _labelY = _geometry._y + 4 + (_geometry._h >> 1) - 8;

  defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
  defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
  tgt->PushModColor(_color);
  mtxi->PushUIMatrix();
  primi.RenderQuadAtZ(
      defmtl.get(),
      tgt,
      ix1,  // x0
      ix2,  // x1
      iy1,  // y0
      iy2,  // y1
      0.0f, // z
      0.0f,
      1.0f, // u0, u1
      0.0f,
      1.0f // v0, v1
  );
  tgt->PopModColor();

  ////////////////////////////////////////
  // draw track data
  ////////////////////////////////////////

  SRasterState rstate;
  rstate.SetBlending(lev2::Blending::ADDITIVE);
  rstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
  auto save_rstate = defmtl->swapRasterState(rstate);
  mtxi->PushMMatrix(mmatrix);

  defmtl->SetUIColorMode(UiColorMode::VTX);
  gbi->DrawPrimitive(
      defmtl.get(), //
      _vtxbuf,
      PrimitiveType::LINES,
      0,
      _numvertices);

  mtxi->PopMMatrix();
  defmtl->swapRasterState(save_rstate);

  ////////////////////////////////////////

  mtxi->PopUIMatrix();
  /*int lablen = _label.length();
  if (lablen) {
    tgt->PushModColor(_textcolor);
    ork::lev2::FontMan::PushFont(_font);
    lev2::FontMan::beginTextBlock(tgt, lablen);
    int sw = lev2::FontMan::stringWidth(lablen);
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        iyc - 6,
        _label.c_str());
    //
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }*/
}
