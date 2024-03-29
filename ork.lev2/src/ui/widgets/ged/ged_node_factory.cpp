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
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/rtti/RTTIX.inl>

ImplementReflectionX(ork::lev2::ged::GedFactoryNode, "GedFactoryNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactory, "GedNodeFactory"); // yes I put this here too ;>

namespace ork::lev2::ged {

void GedNodeFactory::describeX(class_t* clazz) { // factories which create nodes
}

void GedFactoryNode::describeX(class_t* clazz) { // a node which creates objects
}

GedFactoryNode::GedFactoryNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object) {
  _iodriver = iodriver;
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

bool GedFactoryNode::OnMouseDoubleClicked(ui::event_constptr_t ev) {
  auto model      = _container->_model;
  auto skin       = _container->_activeSkin;
  const int klabh = get_charh();
  const int kdim  = klabh - 2;
  int ibasex      = (kdim + 4) * 2 + 3;
  int sx          = ev->miScreenPosX;
  int sy          = ev->miScreenPosY; // - H*2;

  if (not _factory_set.empty()) {
    std::vector<std::string> factory_names;
    for (auto f : _factory_set) {
      factory_names.push_back(f->Name());
    }
    auto dimensions    = ui::ChoiceList::computeDimensions(factory_names);
    std::string choice = ui::popupChoiceList(
        _l2context(), //
        sx,
        sy,
        factory_names,
        dimensions);

    printf("choice<%s>\n", choice.c_str());
    if (choice != "") {
      auto clazz     = rtti::Class::FindClass(choice.c_str());
      auto obj_clazz = dynamic_cast<object::ObjectClass*>(clazz);
      printf("obj_clazz<%p:%s>\n", obj_clazz, obj_clazz->Name().c_str());
      auto instance            = obj_clazz->createShared();
      _iodriver->_abstract_val = instance;
      _iodriver->_onValueChanged();
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////

object_ptr_t invokeFactoryPopup( //
    lev2::Context* ctx,         //
    std::string base_classname, //
    int sx,
    int sy) { //
  object_ptr_t rval;
  auto base_clazz   = rtti::Class::FindClass(base_classname.c_str());
  auto as_obj_clazz = dynamic_cast<ork::object::ObjectClass*>(base_clazz);
  if (as_obj_clazz) {
    std::vector<std::string> factory_names;
    factory_class_set_t factory_set;
    enumerateFactories(as_obj_clazz, factory_set);
    for (auto clazz : factory_set) {
      auto clazz_name = clazz->Name();
      factory_names.push_back(clazz_name);
      printf("!!! FACTORY<%s>\n", clazz_name.c_str());
    }
    auto dimensions    = ui::ChoiceList::computeDimensions(factory_names);
    std::string choice = ui::popupChoiceList(
        ctx, //
        sx,
        sy,
        factory_names,
        dimensions);
    printf("choice<%s>\n", choice.c_str());
    if (choice != "") {
      auto clazz     = rtti::Class::FindClass(choice.c_str());
      auto obj_clazz = dynamic_cast<object::ObjectClass*>(clazz);
      printf("obj_clazz<%p:%s>\n", obj_clazz, obj_clazz->Name().c_str());
      rval = obj_clazz->createShared();
    }
  }
  return rval;
}

////////////////////////////////////////////////////////////////

void enumerateFactories(
    const ork::Object* pdestobj, //
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
            // ObjectFactoryFilter factfilterev;
            // factfilterev.mpClass = pclass;
            // pdestobj->Query(&factfilterev);
            // bok2add &= factfilterev.mbFactoryOK;
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

void enumerateFactories(object::ObjectClass* baseclazz, factory_class_set_t& factory_set) {
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
} // namespace ork::lev2::ged
