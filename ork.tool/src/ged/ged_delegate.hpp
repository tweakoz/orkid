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
