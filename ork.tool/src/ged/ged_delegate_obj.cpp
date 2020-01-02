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
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/Functor.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringOutputStream.h>

#include <orktool/toolcore/dataflow.h>
#include <ork/reflect/Command.h>
#include <QMenu>
#include <QAction>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

template <>
GedObjNode<PropSetterObj>::GedObjNode(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mSetter(prop, obj)
    , mbInteractive(false)
    , mbCollapse(true) {
  ///////////////////////////////////////////
  PersistHashContext HashCtx;
  HashCtx.mObject     = obj;
  HashCtx.mProperty   = prop;
  PersistantMap* pmap = mdl.GetPersistMap(HashCtx);
  ///////////////////////////////////////////

  const std::string& str_collapse = pmap->GetValue("collapse");

  if (str_collapse == "false") {
    mbCollapse = false;
  }

  const reflect::IObjectPropertyType<ork::rtti::ICastable*>* castprop = rtti::autocast(prop);
  const reflect::IObjectPropertyType<ork::Object*>* objprop           = rtti::autocast(prop);

  ork::Object* psubobj = 0;

  ConstString obj_ops;

  if (castprop) {
    ork::rtti::ICastable* castable = 0;

    castprop->Get(castable, obj);

    if (castable) {
      psubobj                       = rtti::downcast<ork::Object*>(castable);
      ork::Object* psubOBJ          = rtti::autocast(psubobj);
      object::ObjectClass* objclass = rtti::downcast<object::ObjectClass*>(psubOBJ->GetClass());

      /*obj_ops = objclass->Description().classAnnotation( "editor.object.ops" );

      mdl.GetGedWidget()->PushItemNode( this );
      const reflect::IObjectFunctor *functor =
      rtti::downcast<object::ObjectClass*>(psubobj->GetClass())->Description().FindFunctor("GetName"); if( functor )
      {
          reflect::IInvokation *invokation = functor->CreateInvokation();
          if(invokation->GetNumParameters() == 0)
          {
              ArrayString<1024> result;

              stream::StringOutputStream ostream(result);
              reflect::serialize::XMLSerializer serializer(ostream);
              reflect::BidirectionalSerializer result_bidi(serializer);

              functor->invoke(psubobj, invokation, &result_bidi);

              ArrayString<1024> original(_propname.c_str());

              MutableString mutstr(_propname);
              mutstr.format("%s : %s", original.c_str(), result.c_str());
          }
          delete invokation;
      }
      ConstString anno = GetOrkProp()->GetAnnotation( "editor.descend" );
      if(anno != "false")
          mdl.Recurse( static_cast<ork::Object*>(castable) );
      mdl.GetGedWidget()->PopItemNode( this );*/
    }

  } else if (objprop) {
    objprop->Get(psubobj, obj);
  }

  /////////////////////////////////////////////////////
  // HAVE SUBOBJ
  /////////////////////////////////////////////////////

  mdl.GetGedWidget()->PushItemNode(this);
  printf("GedObjNode<%s> psubobj<%p> 1\n", name, psubobj);
  if (psubobj) {
    const reflect::IObjectFunctor* functor =
        rtti::downcast<object::ObjectClass*>(psubobj->GetClass())->Description().FindFunctor("GetName");
    if (functor) {
      reflect::IInvokation* invokation = functor->CreateInvokation();
      if (invokation->GetNumParameters() == 0) {
        ArrayString<1024> result;

        stream::StringOutputStream ostream(result);
        reflect::serialize::XMLSerializer serializer(ostream);
        reflect::BidirectionalSerializer result_bidi(serializer);

        functor->invoke(psubobj, invokation, &result_bidi);
        _propname = FormatString("%s : %s", _propname.c_str(), result.c_str());
      }
      delete invokation;
    }
    ConstString anno = GetOrkProp()->GetAnnotation("editor.descend");
    if (anno != "false")
      mdl.Recurse(psubobj);
  }
  mdl.GetGedWidget()->PopItemNode(this);

  ///////////////////////////////////////////////////
  // is this an interactive object node ?
  ///////////////////////////////////////////////////

  ConstString anno = GetOrkProp()->GetAnnotation("editor.choicelist");

  if (anno.length()) {
    ChoiceList* chclist = mModel.GetChoiceManager()->GetChoiceList(anno.c_str());

    if (chclist) {
      mbInteractive = true;
    }
  } else {
    anno = GetOrkProp()->GetAnnotation("editor.factorylistbase");
    // orkset<object::ObjectClass*> Factories;
    // EnumerateFactories( GetOrkProp(), Factories );

    if (anno.length()) {
      mbInteractive = true;
    }
  }
  ///////////////////////////////////////////////////
  // std::string str = ork::CreateFormattedString("%s::Create", name );
}

template <typename Setter> void GedObjNode<Setter>::DoDraw(lev2::Context* pTARG) {
  GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  GetSkin()->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);

  int ity = get_text_center_y();

  if (mbInteractive) {
    GetSkin()->DrawBgBox(this, miX, miY, miW, get_charh(), GedSkin::ESTYLE_BACKGROUND_OBJNODE_LABEL);
    int ilabw = propnameWidth();

    int ioff = 3;
    int idim = (get_charh() - 4);

    int dbx1 = miX + ioff;
    int dbx2 = dbx1 + idim;
    int dby1 = miY + ioff;
    int dby2 = dby1 + idim;

    int labix = (miW >> 1) - (ilabw >> 1);
    if (labix < (dbx2 + 3))
      labix = dbx2 + 3;

    if (mbCollapse) {
      GetSkin()->DrawRightArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
      GetSkin()->DrawLine(this, dbx1 + 1, dby1, dbx1 + 1, dby2, GedSkin::ESTYLE_BUTTON_OUTLINE);
    } else {
      GetSkin()->DrawDownArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
      GetSkin()->DrawLine(this, dbx1, dby1 + 1, dbx2, dby1 + 1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    }
    GetSkin()->DrawText(this, labix, ity, _propname.c_str());
  } else {
    GetSkin()->DrawText(this, miX + 6, ity, _propname.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename Setter> void GedObjNode<Setter>::OnMouseDoubleClicked(const ork::ui::Event& ev) {
  if (mbInteractive) {
    OnCreateObject();
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename Setter> void GedObjNode<Setter>::OnCreateObject() {
  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
  ///////////////////////////////////////////
  ///////////////////////////////////////////
  // mark that we are setting properties from the editor
  ork::PropSetContext pctx(ork::PropSetContext::EPROPEDITOR);
  ///////////////////////////////////////////
  ///////////////////////////////////////////

  Object* NewObject = 0;

  ///////////////////////////////////////////
  // ASSET enumerator
  ///////////////////////////////////////////

  ConstString anno = GetOrkProp()->GetAnnotation("editor.choicelist");

  if (anno.length()) {
    ChoiceList* chclist = 0;
    ;

    chclist = mModel.GetChoiceManager()->GetChoiceList(anno.c_str());

    chclist->EnumerateChoices();

    QMenu qm;

    QAction* pact = qm.addAction("none");
    pact->setData(QVariant("none"));
    QAction* pact2 = qm.addAction("reload");
    pact2->setData(QVariant("reload"));

    if (chclist) {
      QMenu* qm2 = chclist->CreateMenu();

      qm.addMenu(qm2);
    }

    pact = qm.exec(QCursor::pos());

    if (pact) {
      QVariant UserData = pact->data();
      std::string pname = UserData.toString().toStdString();

      const AttrChoiceValue* Chc = chclist->FindFromLongName(pname);

      if (Chc) {
        std::string valuestr = Chc->EvaluateValue();
        PropTypeString tstr(valuestr.c_str());
        NewObject                                                          = PropType<Object*>::FromString(tstr);
        const reflect::IObjectPropertyType<ork::rtti::ICastable*>* objprop = rtti::autocast(GetOrkProp());
        if (objprop && NewObject) {
          mModel.SigPreNewObject();
          objprop->Set(NewObject, GetOrkObj());
          mModel.SigNewObject(NewObject);
        }

      } else if (pname == "none") {
        const reflect::IObjectPropertyType<ork::rtti::ICastable*>* objprop = rtti::autocast(GetOrkProp());
        if (objprop) {
          mModel.SigPreNewObject();
          objprop->Set(0, GetOrkObj());
          mModel.SigNewObject(0);
          // mModel.SigPostNewObject(0);
        }
      }
    }

  } else {
    anno = GetOrkProp()->GetAnnotation("editor.factorylistbase");
    orkset<object::ObjectClass*> Factories;
    EnumerateFactories(GetOrkObj(), GetOrkProp(), Factories);

    if (anno.length()) {
      QMenu* qmenu = CreateFactoryMenu(Factories);

      QAction* pact = qmenu->exec(QCursor::pos());

      if (pact) {
        QVariant UserData                 = pact->data();
        std::string sname                 = UserData.toString().toStdString();
        const char* pname                 = sname.c_str();
        rtti::Class* pclass               = rtti::Class::FindClass(pname);
        ork::object::ObjectClass* poclass = rtti::autocast(pclass);
        if (poclass) {
          NewObject = rtti::autocast(poclass->CreateObject());
        }
        const reflect::IObjectPropertyType<ork::rtti::ICastable*>* castprop = rtti::autocast(GetOrkProp());
        const reflect::IObjectPropertyType<ork::Object*>* objprop           = rtti::autocast(GetOrkProp());
        if (castprop && NewObject) {
          mModel.SigPreNewObject();
          castprop->Set(NewObject, GetOrkObj());
          mModel.SigNewObject(NewObject);
        } else if (objprop && NewObject) {
          mModel.SigPreNewObject();
          objprop->Set(NewObject, GetOrkObj());
          mModel.SigNewObject(NewObject);
        }
      }
    }
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
}

///////////////////////////////////////////////////////////////////////////////

template class GedObjNode<PropSetterObj>;

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
