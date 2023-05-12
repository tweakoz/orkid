////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <QtWidgets/QAction>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <orktool/ged/ged_io.h>

namespace ork { namespace tool { namespace ged {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

template <typename IODriver>
GedFloatNode<IODriver>::GedFloatNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mIoDriver(mdl, prop, obj)
    , mLogMode(false) {
  auto annomin   = prop->annotation("editor.range.min");
  auto annomax   = prop->annotation("editor.range.max");
  auto annolog   = prop->annotation("editor.range.log");
  auto annorange = prop->annotation("editor.range");

  float fval = -1.0f;

  mIoDriver.GetValue(fval);

  float fmin = 0.0f;
  float fmax = 1.0f;

  if (auto as_floatrange = annorange.TryAs<float_range>()) {
    fmin = as_floatrange.value()._min;
    fmax = as_floatrange.value()._max;
  } else {

    if (auto as_float = annomin.TryAs<float>())
      fmin = as_float.value();
    else if (auto as_str = annomin.TryAs<ConstString>())
      sscanf(as_str.value().c_str(), "%f", &fmin);

    if (auto as_float = annomax.TryAs<float>())
      fmax = as_float.value();
    else if (auto as_str = annomax.TryAs<ConstString>())
      sscanf(as_str.value().c_str(), "%f", &fmax);

    else if (auto as_str = annolog.TryAs<ConstString>()) {
      if (as_str.value() == "true") {
        mLogMode = true;
      }
    }
  }

  _slider = new Slider<GedFloatNode>(*this, fmin, fmax, fval);
  _slider->SetLogMode(mLogMode);
}

template <typename IODriver>
void GedFloatNode<IODriver>::ReSync() // virtual
{
  float fval = -1.0f;
  mIoDriver.GetValue(fval);
  _slider->SetVal(fval);
}

template <typename IoDriver> void GedFloatNode<IoDriver>::DoDraw(lev2::Context* pTARG) {
  _slider->resize(miX, miY, miW, miH);

  int ixi = int(_slider->GetIndicPos()) - miX;
  GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  GetSkin()->DrawBgBox(this, miX + 2, miY + 2, miW - 3, miH - 4, GedSkin::ESTYLE_BACKGROUND_2);
  GetSkin()->DrawBgBox(this, miX + 2, miY + 4, ixi, miH - 8, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

  ork::PropTypeString pstr;
  float fval = 0.0f;
  mIoDriver.GetValue(fval);
  float finp          = _slider->GetTextPos();
  int itxi            = miX + (finp);
  PropTypeString& str = _slider->ValString();

  _content     = str.c_str();
  int itextlen = contentWidth();

  int ity = get_text_center_y();

  GetSkin()->DrawText(this, miX + miW - (itextlen + 8), ity, str.c_str());
  GetSkin()->DrawText(this, miX + 4, ity, _propname.c_str());
}

template <typename IoDriver> void GedFloatNode<IoDriver>::onActivate() {
  _slider->onActivate();
}
template <typename IoDriver> void GedFloatNode<IoDriver>::onDeactivate() {
  _slider->onDeactivate();
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver>
GedIntNode<IODriver>::GedIntNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mIoDriver(mdl, prop, obj)
    , mLogMode(false) {
  ConstString annomin = prop->GetAnnotation("editor.range.min");
  ConstString annomax = prop->GetAnnotation("editor.range.max");
  ConstString annolog = prop->GetAnnotation("editor.range.log");

  int ival = -1;

  mIoDriver.GetValue(ival);

  int fmin = 0.0f;
  int fmax = 1.0f;
  if (annomin.length()) {
    sscanf(annomin.c_str(), "%d", &fmin);
  }
  if (annomax.length()) {
    sscanf(annomax.c_str(), "%d", &fmax);
  }

  if (annolog.length()) {
    if (annolog == "true") {
      mLogMode = true;
    }
  }

  _slider   = new Slider<GedIntNode>(*this, fmin, fmax, ival);
  _propname = name;
}

template <typename IODriver>
void GedIntNode<IODriver>::ReSync() // virtual
{
  int ival = 9;
  mIoDriver.GetValue(ival);
  ((Slider<GedIntNode>*)_slider)->SetVal(ival);
}

template <typename IODriver> void GedIntNode<IODriver>::DoDraw(lev2::Context* pTARG) {
  _slider->resize(miX, miY, miW, miH);

  int ixi = int(_slider->GetIndicPos()) - miX;
  GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  GetSkin()->DrawBgBox(this, miX + 2, miY + 2, miW - 3, miH - 4, GedSkin::ESTYLE_BACKGROUND_2);
  GetSkin()->DrawBgBox(this, miX + 2, miY + 3, ixi, miH - 6, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

  int ity = get_text_center_y();

  ork::PropTypeString pstr;
  int ival = 0;
  mIoDriver.GetValue(ival);
  float finp          = _slider->GetTextPos();
  int itxi            = miX + int(finp);
  PropTypeString& str = _slider->ValString();
  GetSkin()->DrawText(this, itxi, ity, str.c_str());
  _content     = str.c_str();
  int itextlen = contentWidth();
  GetSkin()->DrawText(this, miX + 4, ity, _propname.c_str());
  //	GetSkin()->DrawText( this, miX+miW-(itextlen+8), miY+4, str.c_str() );
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedIntNode<IODriver>::OnUiEvent(ork::ui::event_constptr_t ev) {
  _slider->OnUiEvent(ev);
  mModel.SigRepaint();
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedFloatNode<IODriver>::OnUiEvent(ork::ui::event_constptr_t ev) {
  _slider->OnUiEvent(ev);
  mModel.SigRepaint();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver, typename T>
GedSimpleNode<IODriver, T>::GedSimpleNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mIoDriver(mdl, prop, obj) {
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver, typename T> void GedSimpleNode<IODriver, T>::OnUiEvent(ork::ui::event_constptr_t ev) {
  const auto& filtev = ev->mFilteredEvent;

  switch (filtev._eventcode) {
    case ui::EventCode::DOUBLECLICK: {

      T val;

      mIoDriver.GetValue(val);
      PropTypeString ptsg;
      PropType<T>::ToString(val, ptsg);

      ConstString anno_ucdclass = GetOrkProp()->GetAnnotation("ged.userchoice.delegate");

      if (anno_ucdclass.length()) {
        ork::Object* pobj = GetOrkObj();

        rtti::Class* the_class = rtti::Class::FindClass(anno_ucdclass);
        if (the_class) {
          ork::object::ObjectClass* pucdclass = rtti::autocast(the_class);
          ork::rtti::ICastable* ucdo          = pucdclass->CreateObject();
          auto ucd                            = dynamic_cast<IUserChoiceDelegate*>(ucdo);
          if (ucd) {
            auto uchc     = std::make_shared<UserChoices>(*ucd, pobj, this);
            QMenu* qm     = qmenuFromChoiceList(uchc);
            QAction* pact = qm->exec(QCursor::pos());
            if (pact) {
              QVariant UserData = pact->data();
              QString UserName  = UserData.toString();
              std::string pname = UserName.toStdString();

              auto Chc = uchc->FindFromLongName(pname);

              if (Chc) {
                if (Chc->GetCustomData().template IsA<T>()) {
                  const T& value = Chc->GetCustomData().template Get<T>();
                }
                std::string valuestr = Chc->EvaluateValue();
                PropTypeString pts(valuestr.c_str());
                val = PropType<T>::FromString(pts);
                mIoDriver.SetValue(val);
              }
            }
          }
        }
      } else {
        int ilabw        = propnameWidth() + 16;
        QString qstr     = GedInputDialog::getText(ev, this, ptsg.c_str(), ilabw, 2, miW - ilabw, miH - 3);
        std::string sstr = qstr.toStdString();
        if (sstr.length()) {
          PropTypeString pts(sstr.c_str());
          val = PropType<T>::FromString(pts);
          mIoDriver.SetValue(val);
        }
      }
    } break;
  }
  SigInvalidateProperty();
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver, typename T> void GedSimpleNode<IODriver, T>::DoDraw(lev2::Context* pTARG) {
  bool isPickState = pTARG->FBI()->isPickState();

  int ity = get_text_center_y();

  int ilabw = propnameWidth() + 16;
  // GedItemNode::DoDraw( pTARG );
  //////////////////////////////////////
  T val;
  mIoDriver.GetValue(val);
  GetSkin()->DrawBgBox(this, miX + ilabw, miY, miW - ilabw, miH, GedSkin::ESTYLE_BACKGROUND_2);

  if (false == isPickState) {
    PropTypeString pts;
    PropType<T>::ToString(val, pts);
    //////////////////////////////////////

    GetSkin()->DrawText(this, miX + ilabw, ity, pts.c_str());
    GetSkin()->DrawText(this, miX + 6, ity, _propname.c_str());
  }
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
