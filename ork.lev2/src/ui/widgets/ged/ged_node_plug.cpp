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
#include <ork/dataflow/plug_inst.h>
#include <ork/dataflow/module.h>
#include "ged_slider.inl"

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

class PlugInputConnectHitBox : public GedObject {
  DeclareAbstractX(PlugInputConnectHitBox, GedObject);
  PlugInputConnectHitBox(GedPlugNode* node)
      : _node(node) {
  }
  bool OnUiEvent(ui::event_constptr_t ev) final;
  GedPlugNode* _node = nullptr;
};
////////////////////////////////////////////////////////////////
void PlugInputConnectHitBox::describeX(class_t* clazz) {
}
////////////////////////////////////////////////////////////////
bool PlugInputConnectHitBox::OnUiEvent(ui::event_constptr_t ev) {
  const auto& filtev = ev->mFilteredEvent;
  bool rval          = false;
  switch (ev->_eventcode) {
    case ui::EventCode::DOUBLECLICK: {
      
      auto ged_container = _node->_container;
      auto ged_model = ged_container->_model;

      auto the_input_plug = std::dynamic_pointer_cast<dataflow::InPlugData>(_node->_object);
      auto input_module = the_input_plug->_parent_module;
      auto gdata = input_module->_graphdata;

      OrkAssert(the_input_plug);
      OrkAssert(input_module);
      OrkAssert(gdata);

      std::vector<std::string> choices;
      std::map<std::string, dataflow::floatoutplug_ptr_t> choice_to_module;

      choices.push_back("NONE");
      choice_to_module["NONE"] = nullptr;
      auto input_as_floatxf = std::dynamic_pointer_cast<dataflow::floatxfinplugdata_t>(the_input_plug);

      for( auto item : gdata->_modules ) { //
        auto test_module_name = item.first;
        printf( "TESTING MODULE<%s>\n", test_module_name.c_str() );
        auto test_module_data = std::dynamic_pointer_cast<dataflow::ModuleData>(item.second);
        if( test_module_data != input_module ){ //
          for( auto test_output_plug : test_module_data->_outputs ){ //
              auto plugname = test_output_plug->_name;
            printf( "  TESTING PLUG<%s>\n", plugname.c_str() );

            auto output_as_float = std::dynamic_pointer_cast<dataflow::floatoutplug_t>(test_output_plug);

            //bool can_connect = input_as_floatxf and output_as_float;
            bool can_connect = gdata->canConnect(input_as_floatxf, output_as_float);

            if( can_connect ){ //
              auto mname = FormatString("%s.%s", test_module_name.c_str(), plugname.c_str());
              choices.push_back(mname);
              choice_to_module[mname] = output_as_float;
            }
          }
        }
      }
      if( choices.size() ){
          fvec2 dimensions   = ui::ChoiceList::computeDimensions(choices);
          int sx             = ev->miScreenPosX;
          int sy             = ev->miScreenPosY;
          std::string choice = ui::popupChoiceList(
              _node->_l2context(), //
              sx - (int(dimensions.x) >> 1),
              sy - (int(dimensions.y) >> 1),
              choices,
              dimensions);
          printf("choice<%s>\n", choice.c_str());
          auto it_choice = choice_to_module.find(choice);
          OrkAssert(it_choice!=choice_to_module.end());
          auto output_plug = it_choice->second;
          if( output_plug ){
            gdata->safeConnect(input_as_floatxf, output_plug);
          }
          else{
            gdata->disconnect(input_as_floatxf);
          }
          ged_model->enqueueUpdate();

      }
      rval = true;
      break;
    }
    default:
      break;
  }
  return rval;
}
////////////////////////////////////////////////////////////////
using pluginputconnecthitbox_ptr_t = std::shared_ptr<PlugInputConnectHitBox>;
struct FloatPlugXfEditorImpl {

  using datatype             = float;
  static const int kpoolsize = 32;

  FloatPlugXfEditorImpl(GedPlugNode* node);
  void render(lev2::Context* pTARG);
  bool onUiEvent(ui::event_constptr_t ev);

  GedPlugNode* _node                            = nullptr;
  dataflow::floatxfinplugdata_t* _inputPlugData = nullptr;
  Slider<float> _floatSlider;
  pluginputconnecthitbox_ptr_t _connectHitBox;
  bool _is_connected = false;
};

using floatplugxfeditorimpl_ptr_t = std::shared_ptr<FloatPlugXfEditorImpl>;

///////////////////////////////////////////////////////////////////////////////

FloatPlugXfEditorImpl::FloatPlugXfEditorImpl(GedPlugNode* node)
    : _node(node)
    , _floatSlider(_node, 0.0f, 1.0f, 0.0f) {
  auto c         = _node->_container;
  auto model     = c->_model;
  auto obj       = _node->_object;
  auto prop      = _node->_property;
  _inputPlugData = dynamic_cast<dataflow::floatxfinplugdata_t*>(obj.get());

  if (_inputPlugData) {

    _connectHitBox            = std::make_shared<PlugInputConnectHitBox>(_node);
    _connectHitBox->_propname = "connectBOX";

    _is_connected = _inputPlugData->isConnected();

    /////////////////////////////////////////
    // slider
    /////////////////////////////////////////

    if( not _is_connected ){

      auto sl_iodriver = std::make_shared<NewIoDriver>();
      sl_iodriver->_par_prop = prop;
      sl_iodriver->_object   = obj;
      //sl_iodriver->_abstract_val.set<float>(_inputPlugData->value());
      _floatSlider._iodriver = sl_iodriver;
      _floatSlider.SetVal(_inputPlugData->value());
      sl_iodriver->_onValueChanged = [=]() {
        _inputPlugData->setValue(  _floatSlider.value() );
      };

    }

    /////////////////////////////////////////
    // transform
    /////////////////////////////////////////

    c->PushItemNode(_node);

    auto xf_iodriver = std::make_shared<NewIoDriver>();
    // auto ioimpl               = iodriver->_impl.makeShared<ArrayIoDriverImpl>();
    // ioimpl->_node             = this;
    // ioimpl->_array_prop       = ary_prop;
    // ioimpl->_index            = index++;
    xf_iodriver->_par_prop = prop;
    xf_iodriver->_object   = _inputPlugData->_transformer;
    xf_iodriver->_abstract_val.set<object_ptr_t>(_inputPlugData->_transformer);
    xf_iodriver->_onValueChanged = [=]() {
      // ary_prop->setElement(obj, key, xf_iodriver->_abstract_val);
      c->_model->enqueueUpdate();
    };

    auto item_node = model->createAbstractNode("Transform", xf_iodriver);
    c->PopItemNode(_node);

    /////////////////////////////////////////
  }
}

///////////////////////////////////////////////////////////////////////////////

void FloatPlugXfEditorImpl::render(lev2::Context* pTARG) {

  auto c       = _node->_container;
  auto model   = c->_model;
  auto skin    = c->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(_node, _node->miX, _node->miY, _node->miW, _node->miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  //////////////////////////////////////

  const int kcharw   = _node->get_charw();
  const int klabh    = _node->get_charh();
  const int kcharwd2 = (kcharw >> 1);

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

  if (true) {
    if (_node->mbcollapsed) {
      skin->DrawRightArrow(_node, dbx1, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    } else {
      skin->DrawDownArrow(_node, dbx1, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    }

    dbx1 += (idim + 4);
    dbx2 = dbx1 + idim;
  }

  // skin->DrawOutlineBox(_node, dbx1, _node->miY, dbw, igdim, GedSkin::ESTYLE_DEFAULT_OUTLINE);

  if (_inputPlugData) {
    if (is_pick) {
      skin->DrawBgBox(_connectHitBox.get(), dbx1, dby1, idim, idim, GedSkin::ESTYLE_BACKGROUND_2);
    } else {
      for (int i = 0; i <= 6; i += 3) {
        int j = (i * 2);
        skin->DrawOutlineBox(_node, dbx1 + i, dby1 + i, idim - j, idim - j, GedSkin::ESTYLE_BUTTON_OUTLINE);
      }
    }
    dbx1 += ifdim;
    int dbw = _node->miW - (dbx1 - _node->miX);

    int header_y = _node->miY + 3;
    int header_h = igdim - 4;

    //////////////////////////////////////
    // plugrate BG
    //////////////////////////////////////

    int plugrate_x = dbx1 + 5 - kcharwd2;
    int plugrate_w = kcharw * 3 - 1;

    skin->DrawBgBox(
        _node,      //
        plugrate_x, // x
        header_y,   // y
        plugrate_w, // w
        header_h,   // h
        GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

    skin->DrawOutlineBox(
        _node,      //
        plugrate_x, // x
        header_y,   // y
        plugrate_w, // w
        header_h,   // h
        GedSkin::ESTYLE_DEFAULT_OUTLINE);

    //////////////////////////////////////
    // plugrate
    //////////////////////////////////////

    const char* ptstr = "??";
    switch (_inputPlugData->_plugrate) {
      case dataflow::EPR_EVENT:
        ptstr = "EV";
        break;
      case dataflow::EPR_UNIFORM:
        ptstr = "UN";
        break;
      case dataflow::EPR_VARYING1:
        ptstr = "V1";
        break;
      case dataflow::EPR_VARYING2:
        ptstr = "V2";
        break;
    }

    skin->DrawText(_node, dbx1 + 4, _node->miY + 1, ptstr);

    // dbx1 += kcharw * 3;
    // dbw -= kcharw * 3;

    //////////////////////////////////////

    auto in_float_plugdata = dynamic_cast<dataflow::floatxfinplugdata_t*>(_inputPlugData);
    float ity           = _node->get_text_center_y();
    int slider_x = plugrate_x + plugrate_w + 3;
    int slider_w = _node->miW - (slider_x + 2 - _node->miX);

    if( _is_connected ){
      auto conplug = _inputPlugData->_connectedOutput;
      auto conmod  = conplug->_parent_module;
      auto constr = FormatString("CONNECTED TO<%s.%s>", conmod->_name.c_str(), conplug->_name.c_str());
      skin->DrawBgBox(_node, slider_x, header_y, slider_w, header_h, GedSkin::ESTYLE_BACKGROUND_3);
      skin->DrawText(_node, slider_x, ity, constr.c_str());
    }
    else if (in_float_plugdata) {
      _floatSlider.resize(slider_x, _node->miY, slider_w, _node->miH - 8);

      skin->DrawBgBox(_node, slider_x, header_y, slider_w, header_h, GedSkin::ESTYLE_BACKGROUND_2);
      if (is_pick) {
      } else if (1) {
        int slider_dd = int(_floatSlider.GetIndicPos()) - slider_x;

        skin->DrawBgBox(_node, slider_x, header_y + 1, slider_dd - 4, header_h - 2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

        ork::PropTypeString pstr;
        float fvalue        = _floatSlider.value();
        float finp          = _floatSlider.GetTextPos();
        int itxi            = _node->miX + (finp);
        PropTypeString& str = _floatSlider.ValString();
        skin->DrawText(_node, itxi, ity, str.c_str());
      }
    }
  }
}

bool FloatPlugXfEditorImpl::onUiEvent(ui::event_constptr_t ev) {
  bool handled = false;
  switch (ev->_eventcode) {
    case ui::EventCode::DOUBLECLICK: {
      break;
    }
    default:
      break;
  }
  if (not handled) {
    handled = _floatSlider.OnUiEvent(ev);
  }
  return handled;
}

///////////////////////////////////////////////////////////////////////////////

void GedPlugNode::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

GedPlugNode::GedPlugNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver) {
  auto as_float_xf = dynamic_cast<dataflow::floatxfinplugdata_t*>(_object.get());
  if (as_float_xf) {
    auto pei = _impl.makeShared<FloatPlugXfEditorImpl>(this);
  } else {
    printf("GedPlugNode<%p> not floatxfinplugdata_t\n", this);
    printf(" clazz<%s>\n", _object->objectClass()->Name().c_str());
  }
}

////////////////////////////////////////////////////////////////

void GedPlugNode::DoDraw(lev2::Context* pTARG) {
  auto pei = _impl.tryAs<floatplugxfeditorimpl_ptr_t>();
  if (pei) {
    pei.value()->render(pTARG);
  }
}

////////////////////////////////////////////////////////////////

int GedPlugNode::doComputeHeight() const {
  return GedItemNode::doComputeHeight();
}

////////////////////////////////////////////////////////////////

bool GedPlugNode::OnUiEvent(ui::event_constptr_t ev) {
  auto pei = _impl.tryAs<floatplugxfeditorimpl_ptr_t>();
  if (pei) {
    return pei.value()->onUiEvent(ev);
  } else {
    return false;
  }
}

////////////////////////////////////////////////////////////////

GedNodeFactoryPlug::GedNodeFactoryPlug() {
}

geditemnode_ptr_t
GedNodeFactoryPlug::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedPlugNode>(container, Name.c_str(), iodriver);
}

///////////////////////////////////////////////////////////////////////////////

struct GedPlugFloatXfNode : public GedItemNode {
  DeclareAbstractX(GedPlugFloatXfNode, GedItemNode);

public:
  ///////////////////////////////////////////////////

  GedPlugFloatXfNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
      : GedItemNode(c, name, iodriver) {
  }
  int doComputeHeight() const final {
    return 32;
  }
  void DoDraw(lev2::Context* pTARG) final {
  }
  bool OnUiEvent(ork::ui::event_constptr_t ev) final {
    return false;
  }
};

void GedPlugFloatXfNode::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////
geditemnode_ptr_t //
GedNodeFactoryPlugFloatXF::createItemNode(GedContainer* c, const ConstString& name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedPlugFloatXfNode>(c, name.c_str(), iodriver);
}

GedNodeFactoryPlugFloatXF::GedNodeFactoryPlugFloatXF() {
}

void GedNodeFactoryPlugFloatXF::describeX(class_t* clazz) {
}

void GedNodeFactoryPlug::describeX(class_t* clazz) {
  // GedNodeFactoryPlugFloatXF::GetClassStatic();
  GedPlugFloatXfNode::GetClassStatic();
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedPlugNode, "GedPlugNode");
ImplementReflectionX(ork::lev2::ged::GedPlugFloatXfNode, "GedPlugFloatXfNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryPlug, "GedNodeFactoryPlug");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryPlugFloatXF, "GedNodeFactoryPlugFloatXF");
ImplementReflectionX(ork::lev2::ged::PlugInputConnectHitBox, "PlugInputConnectHitBox");
