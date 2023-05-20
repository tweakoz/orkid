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
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/ui/popups.inl>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedAssetNode::describeX(class_t* clazz) {
}

GedAssetNode::GedAssetNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver) {
}

////////////////////////////////////////////////////////////////

void GedAssetNode::DoDraw(lev2::Context* pTARG) {
  auto model    = _container->_model;
  auto skin     = _container->_activeSkin;
  bool is_pick  = skin->_is_pickmode;
  auto prop     = _iodriver->_par_prop;
  auto instance = _iodriver->_object;

  using prop_t    = reflect::DirectObject<asset::asset_ptr_t>;
  auto typed_prop = dynamic_cast<const prop_t*>(prop);

  OrkAssert(typed_prop);

  asset::asset_ptr_t the_asset;
  typed_prop->get(the_asset, instance);
  int pnamew = propnameWidth() + 16;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if (not is_pick) {
    skin->DrawText(this, miX+ 4, miY, _propname.c_str());
  }
  skin->DrawBgBox(this, miX + pnamew, miY, miW - pnamew, miH, GedSkin::ESTYLE_BACKGROUND_2);
  if (the_asset) {
    skin->DrawText(this, miX + pnamew, miY + 2, the_asset->_name.c_str());
  } else {
    skin->DrawText(this, miX + pnamew, miY + 2, "NONE");
  }
}

////////////////////////////////////////////////////////////////

bool GedAssetNode::OnUiEvent(ui::event_constptr_t ev) {
  int sx = ev->miScreenPosX;
  int sy = ev->miScreenPosY;

  switch (ev->_eventcode) {
    case ui::EventCode::DOUBLECLICK: {

      auto prop = _iodriver->_par_prop;
      auto instance = _iodriver->_object;
      using prop_t    = reflect::DirectObject<asset::asset_ptr_t>;
      auto typed_prop = dynamic_cast<const prop_t*>(prop);
      auto asset_class = prop->annotation("editor.asset.class").get<ConstString>();
      OrkAssert(typed_prop);

      std::vector<std::string> choices;
      std::map<std::string,util::choice_ptr_t> choicemap;
      auto chclist = choicemanager()->choicelist(asset_class.c_str());
      chclist->EnumerateChoices();
      for( util::choice_ptr_t chc : chclist->mChoicesVect ){
        auto name = chc->mName;
        auto val = chc->mValue;
        choicemap[name] = chc;
        choices.push_back(name);
      }
      fvec2 dimensions = ui::ChoiceList::computeDimensions(choices);
      printf( "dimensions<%g %g>\n", dimensions.x, dimensions.y);
      std::string choice = ui::popupChoiceList(
          this->_l2context(), //
          sx - (int(dimensions.x)>>1),
          sy - (int(dimensions.y)>>1),
          choices,
          dimensions);
      printf("choice<%s>\n", choice.c_str());
      auto it = choicemap.find(choice);
      if(it!=choicemap.end()){
        auto chc = it->second;
        auto asset = chclist->provideSelection(chc->mValue).get<asset::asset_ptr_t>();
        typed_prop->set(asset, instance);
      }
      break;
    }
    default:
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryAssetList::describeX(class_t* clazz) {
}

geditemnode_ptr_t
GedNodeFactoryAssetList::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedAssetNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedAssetNode, "GedAssetNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryAssetList, "GedNodeFactoryAssetList");
