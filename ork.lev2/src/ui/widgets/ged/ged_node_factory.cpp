////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/kernel/core_interface.h>
#include <ork/rtti/RTTIX.inl>

// template class ork::object::Signal<void,ork::lev2::ged::ObjModel>;

ImplementReflectionX(ork::lev2::ged::GedFactoryNode, "GedFactoryNode");

namespace ork::lev2::ged {

void GedFactoryNode::describeX(class_t* clazz) {
}

GedFactoryNode::GedFactoryNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj)
    : GedItemNode(c, name, prop, obj) {
}

////////////////////////////////////////////////////////////////

void GedFactoryNode::DoDraw(lev2::Context* pTARG) {
  auto model   = _container->_model;
  auto skin    = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if (not is_pick) {
    skin->DrawText(this, miX, miY, _propname.c_str());
  }
}

////////////////////////////////////////////////////////////////

void enumerateFactories(const ork::Object* pdestobj, //
                        const reflect::ObjectProperty* prop, 
                        factory_class_set_t& factory_set) {

  ConstString anno = prop->GetAnnotation("editor.factorylistbase");

  orkvector<std::string> Classes;
  SplitString(anno.c_str(), Classes, " ");
  int inumclasses = int(Classes.size());

  if (inumclasses) {
    for (int i = 0; i < inumclasses; i++) {
      const std::string& ps = Classes[i];

      rtti::Class* pclass = rtti::Class::FindClass(ps.c_str());

      object::ObjectClass* pobjclass = rtti::downcast<object::ObjectClass*>(pclass);

      //////////////////////////////////////////////

      OrkAssert(pobjclass);

      orkstack<object::ObjectClass*> FactoryStack;

      FactoryStack.push(pobjclass);

      while (FactoryStack.empty() == false) {
        object::ObjectClass* pclass = FactoryStack.top();
        FactoryStack.pop();

        if (pclass->hasFactory()) {
          //////////////////////////////////////////////
          // check if class marked uninstantiable by editor
          //////////////////////////////////////////////

          auto instanno = pclass->Description().classAnnotation("editor.instantiable");

          bool bok2add = instanno.isA<bool>() ? instanno.get<bool>() : true;

          if (pdestobj) // check if also OK with the destination object
          {
            //ObjectFactoryFilter factfilterev;
            //factfilterev.mpClass = pclass;
            //pdestobj->Query(&factfilterev);
            //bok2add &= factfilterev.mbFactoryOK;
          }

          if (bok2add) {
            // pact = qm.addAction( pclass->Name().c_str() );
            // pact->setData( QVariant( pclass->Name().c_str() ) );

            factory_set.insert(pclass);
          }
        }

        rtti::Class* const pfirstchild = pclass->FirstChild();
        rtti::Class* pchild            = pfirstchild;

        while (pchild) {
          object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass*>(pchild);
          OrkAssert(pobjchildclass);
          FactoryStack.push(pobjchildclass);
          pchild = (pchild->NextSibling() == pfirstchild) ? 0 : pchild->NextSibling();
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void enumerateFactories(object::ObjectClass* baseclazz, 
                        factory_class_set_t& factory_set) {
  OrkAssert(baseclazz);
  orkstack<object::ObjectClass*> FactoryStack;
  FactoryStack.push(baseclazz);
  while (FactoryStack.empty() == false) {
    object::ObjectClass* pclass = FactoryStack.top();
    FactoryStack.pop();
    if (pclass->hasFactory()) { //////////////////////////////////////////////
      // check if class marked uninstantiable by editor
      //////////////////////////////////////////////

      auto instanno = pclass->Description().classAnnotation("editor.instantiable");

      bool bok2add = instanno.isA<bool>() ? instanno.get<bool>() : true;

      /*ConstString instanno = pclass->Description().classAnnotation( "editor.instantiable" );
      bool bok2add = true;
      if( instanno.length() )
      {	if( instanno == "false" )
          {
              bok2add = false;
          }
      }*/
      if (bok2add) {
        if (pclass->hasFactory()) {
          factory_set.insert(pclass);
        }
      }
    }
    rtti::Class* const pfirstchild = pclass->FirstChild();
    rtti::Class* pchild            = pfirstchild;
    while (pchild) {
      object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass*>(pchild);
      OrkAssert(pobjchildclass);
      FactoryStack.push(pobjchildclass);
      pchild = (pchild->NextSibling() == pfirstchild) ? 0 : pchild->NextSibling();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {

//ImplementReflectionX(ork::lev2::ged::GedFactory, "GedFactory");
