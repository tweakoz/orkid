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
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/ui/popups.inl>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/plug_data.h>
#include "ged_slider.inl"

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

struct FloatPlugEditorImpl {

  using datatype = float;
  static const int kpoolsize = 32;

  FloatPlugEditorImpl(GedPlugNode* node);
  void render(lev2::Context* pTARG);
  bool onUiEvent(ui::event_constptr_t ev);

  GedPlugNode* _node                   = nullptr;
  dataflow::InPlugData* _inputPlugData = nullptr;
  Slider<float> _floatSlider;
};

///////////////////////////////////////////////////////////////////////////////

FloatPlugEditorImpl::FloatPlugEditorImpl(GedPlugNode* node)
    : _node(node)
    , _floatSlider(_node, 0.0f, 1.0f, 0.0f)
 {
  auto obj       = _node->_object;
  auto prop      = _node->_property;
  _inputPlugData = dynamic_cast<dataflow::InPlugData*>(obj.get());
}

///////////////////////////////////////////////////////////////////////////////

void FloatPlugEditorImpl::render(lev2::Context* pTARG) {
  auto c       = _node->_container;
  auto model   = c->_model;
  auto skin    = c->_activeSkin;
  bool is_pick = skin->_is_pickmode;
  //skin->DrawBgBox(_node, x, y, w, h, GedSkin::ESTYLE_BACKGROUND_1, 100);

    //////////////////////////////////////

    const int klabh = _node->get_charh();
    int ioff  = 3;
    int idim  = (klabh);
    int ifdim = klabh;
    int igdim = klabh + 4;

    //////////////////////////////////////

    int dbx1 = _node->miX + ioff;
    int dbx2 = dbx1 + idim;
    int dby1 = _node->miY + ioff;
    int dby2 = dby1 + idim;

    int labw = _node->propnameWidth() + 16;

    //////////////////////////////////////

    if (false) {
      if (_node->mbcollapsed) {
        skin->DrawRightArrow(_node, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
        skin->DrawLine(_node, dbx1 + 1, dby1, dbx1 + 1, dby2, GedSkin::ESTYLE_BUTTON_OUTLINE);
      } else {
        skin->DrawDownArrow(_node, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
        skin->DrawLine(_node, dbx1, dby1 + 1, dbx2, dby1 + 1, GedSkin::ESTYLE_BUTTON_OUTLINE);
      }

      dbx1 += (idim + 4);
      dbx2 = dbx1 + idim;
    }


    //skin->DrawOutlineBox(_node, dbx1, _node->miY, dbw, igdim, GedSkin::ESTYLE_DEFAULT_OUTLINE);

  if(_inputPlugData){
    for (int i = 0; i <= 6; i += 3) {
      int j = (i * 2);
      skin->DrawOutlineBox(_node, dbx1 + i, dby1 + i, idim - j, idim - j, GedSkin::ESTYLE_BUTTON_OUTLINE);
    }
    dbx1 += ifdim;
    int dbw = _node->miW - (dbx1 - _node->miX);

    auto in_float_plugdata = dynamic_cast<dataflow::floatinplugdata*>(_inputPlugData);
    if(in_float_plugdata){
        _floatSlider.resize(dbx1, _node->miY, dbw, _node->miH);
        int ixi = int(_floatSlider.GetIndicPos()) - dbx1;
        skin->DrawBgBox(_node, dbx1, _node->miY, dbw, igdim, GedSkin::ESTYLE_BACKGROUND_1);
        skin->DrawBgBox(_node, dbx1 + 2, _node->miY + 2, dbw - 3, igdim - 4, GedSkin::ESTYLE_BACKGROUND_2);
        skin->DrawBgBox(_node, dbx1 + 2, _node->miY + 4, ixi, igdim - 8, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

        ork::PropTypeString pstr;
        float fvalue = _floatSlider.value();
        float finp          = _floatSlider.GetTextPos();
        int itxi            = _node->miX + (finp);
        float ity = _node->get_text_center_y();
        PropTypeString& str = _floatSlider.ValString();
        skin->DrawText(_node, itxi, ity, str.c_str());
        //_content     = str.c_str();
        //int itextlen = contentWidth();
        //skin->DrawText(this, dbx1 + dbw - (itextlen + 8), miY + 4, str.c_str());

    }
    else{

    }

  }

  // if (not is_pick) {
  // skin->DrawText(_node, x, y, _node->_propname.c_str());
  //}
}

bool FloatPlugEditorImpl::onUiEvent(ui::event_constptr_t ev) {
  return _floatSlider.OnUiEvent(ev);
  /*int sx = ev->miScreenPosX;
  int sy = ev->miScreenPosY;
  switch (ev->_eventcode) {
    case ui::EventCode::DOUBLECLICK: {
      std::vector<std::string> choices;
      choices.push_back("Plug1");
      choices.push_back("Plug2");
      fvec2 dimensions   = ui::ChoiceList::computeDimensions(choices);
      std::string choice = ui::popupChoiceList(
          this->_l2context(), //
          sx - int(dimensions.x) >> 1,
          sy - int(dimensions.y) >> 1,
          choices,
          dimensions);
      printf("choice<%s>\n", choice.c_str());
      break;
    }
    default:
      break;
  }
  return true;*/
}

///////////////////////////////////////////////////////////////////////////////

void GedPlugNode::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

GedPlugNode::GedPlugNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver) {
  auto pei = _impl.makeShared<FloatPlugEditorImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedPlugNode::DoDraw(lev2::Context* pTARG) {
  auto pei = _impl.getShared<FloatPlugEditorImpl>();
  pei->render(pTARG);
}

////////////////////////////////////////////////////////////////

int GedPlugNode::doComputeHeight() const {
  return 32;
}

////////////////////////////////////////////////////////////////

bool GedPlugNode::OnUiEvent(ui::event_constptr_t ev) {
  auto pei = _impl.getShared<FloatPlugEditorImpl>();
  return pei->onUiEvent(ev);
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryPlug::describeX(class_t* clazz) {
}

GedNodeFactoryPlug::GedNodeFactoryPlug() {
}

geditemnode_ptr_t
GedNodeFactoryPlug::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedPlugNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedPlugNode, "GedPlugNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryPlug, "GedNodeFactoryPlug");
