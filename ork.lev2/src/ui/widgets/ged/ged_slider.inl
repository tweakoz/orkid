#pragma once

#include <ork/lev2/ui/popups.inl>

namespace ork::lev2::ged {

template <typename T>
Slider<T>::Slider(T& ParentW, datatype min, datatype max, datatype def)
    : SliderBase()
    , _parent(ParentW)
    , mval(def)
    , mmin(min)
    , mmax(max)
    , mbUpdateOnDrag(false) {
  PropType<datatype>::ToString(mval, mValStr);

  datatype val = def;

  if (val < mmin)
    val = mmin;
  if (val > mmax)
    val = mmax;

  float fx = mlogmode ? ValToLog(mval) : ValToLin(mval);

  PropTypeString outstr;
  PropType<datatype>::ToString(val, outstr);

  mval = val;

  Refresh();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> float Slider<T>::LogToVal(float flog) const {
  const float klogoffset = fabs(float(mmax) - float(mmin)) / 100.0f;
  float flogmin          = log10f(klogoffset + float(mmin));
  float flogmax          = log10f(klogoffset + float(mmax));
  float flogrange        = flogmax - flogmin;

  float flogdist  = (flog * flogrange);
  float flogdist2 = flogdist + flogmin;

  // 10^0.0413927 = 1.1
  // 10^1.0413927 = 11

  float flin = powf(10.0f, flogdist2);

  return flin - klogoffset;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> float Slider<T>::ValToLog(float val) const {
  const float klogoffset = fabs(float(mmax) - float(mmin)) / 100.0f;
  float flogmin          = log10f(klogoffset + float(mmin));
  float flogmax          = log10f(klogoffset + float(mmax));
  float flogrange        = flogmax - flogmin;

  float flogval  = log10f(klogoffset + float(val));
  float flogdist = (flogval - flogmin);

  float fi = flogdist / flogrange;

  return fi;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> float Slider<T>::LinToVal(float lin) const {
  float frange = float(mmax - mmin);
  float fval   = (lin * frange) + mmin;
  return fval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> float Slider<T>::ValToLin(float val) const {
  float frange = float(mmax - mmin);
  float flin   = (float(val) - float(mmin)) / frange;
  return flin;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Slider<T>::Refresh() {
  float fx = mlogmode ? ValToLog(mval) : ValToLin(mval);

  mfIndicPos = (fx * mfw) + mfx;

  float ftextX = 0.0f;
  if (fx < 0.6f) {
    ftextX = 0.66f;
  } else {
    ftextX = 0.16f;
  }

  mfTextPos = (ftextX * mfw) + mfx;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Slider<T>::SetVal(datatype val) {
  if (val < mmin)
    val = mmin;
  if (val > mmax)
    val = mmax;
  mval = val;
  PropType<datatype>::ToString(mval, mValStr);
  _parent._iodriver->_abstract_val.template set<datatype>(mval);
  _parent._iodriver->_onValueChanged();
  Refresh();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Slider<T>::OnUiEvent(ork::ui::event_constptr_t ev) // final
{
  const auto& filtev = ev->mFilteredEvent;

  switch (filtev._eventcode) {
    case ui::EventCode::PUSH: {
      break;
    }
    case ui::EventCode::DRAG: {
      mbUpdateOnDrag = ev->mbCTRL;


      bool bleft  = ev->IsButton0DownF();
      bool bright = ev->IsButton2DownF();

      if (bleft || bright) {
        int mousepos = ev->miX;

        float fx    = float(mousepos - mfx) / (mfw-1);
        if(fx<0.0)
            fx = 0.0f;
        else if(fx>1.0f)
            fx = 1.0f;
        float fval  = mlogmode ? LogToVal(fx) : LinToVal(fx);
        datatype dx = datatype(fval);

        if (bright) {
          dx = datatype(float(mval) * 0.9f + float(dx) * 0.1f);
        }

        mval = dx;
        if (mval < mmin)
          mval = mmin;
        if (mval > mmax)
          mval = mmax;
        PropType<datatype>::ToString(mval, mValStr);
        Refresh();

        //printf("mousepos<%f> mfx<%f> fx<%g> dx<%g> mval<%g>\n", (float)mousepos, mfx, fx, dx, mval);

        if (mbUpdateOnDrag) {
          SetVal(mval);
          //IoDriverBase& iod = _parent.RefIODriver();
          //_parent.SigInvalidateProperty();
        }
      }
      break;
    }
    case ui::EventCode::DOUBLECLICK: {
      int sx = ev->miScreenPosX-(ev->miX-_parent.miX);
      int sy = ev->miScreenPosY;
      int W = _parent.miW;
      int H = _parent.miH;
      datatype ival = _parent._iodriver->_abstract_val.template get<datatype>();
      //std::string initial_val = FormatString("%g", ival);
      PropType<datatype>::ToString(mval, mValStr);
      std::string edittext = ui::popupLineEdit(_parent._l2context(),sx,sy,W,H,mValStr.c_str());
      if (edittext.length()) {
          mval = PropType<datatype>::FromString(edittext.c_str());
          SetVal(mval);
      }

      break;
    }
  }
  SetVal(mval);
  //IoDriverBase& iod = _parent.RefIODriver();
  //_parent.SigInvalidateProperty();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Slider<T>::resize(int ix, int iy, int iw, int ih) {
  mfx = ix;
  mfw = iw;
  mfh = ih;
  Refresh();
}

} // namespace ork::lev2::ged