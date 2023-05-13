////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/orkpool.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/math/gradient.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

static const int kpntsize = 5;

class GedGradientEditPoint : public GedObject {
  DeclareAbstractX(GedGradientEditPoint, GedObject);

  gradient_fvec4_t* mGradientObject;
  GedItemNode* _parent;
  int miPoint;

public:
  void SetPoint(int idx) {
    miPoint = idx;
  }

  GedGradientEditPoint()
      : mGradientObject(0)
      , _parent(0)
      , miPoint(-1) {
  }

  void SetGradientObject(gradient_fvec4_t* pgrad) {
    mGradientObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    _parent = ppar;
  }

  void OnUiEvent(ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    using data_t = orklut<float, fvec4>;

    switch (filtev._eventcode) {
      case ui::EventCode::DRAG: {
        if (_parent && mGradientObject) {
          data_t& data         = mGradientObject->_data;
          const int knumpoints = (int)data.size();
          const int ksegs      = knumpoints - 1;

          if (miPoint > 0 && miPoint < (knumpoints - 1)) {
            int mousepos = ev->miX - _parent->GetX();
            float fx     = float(mousepos) / float(_parent->width());

            data_t::iterator it  = data.begin() + miPoint;
            data_t::iterator itp = it - 1;
            data_t::iterator itn = it + 1;

            if (fx > 0.0f && fx < 1.0f) {
              const float kfbound = float(kpntsize) / _parent->width();
              if (itp != data.end()) {
                if (fx < (itp->first + kfbound)) {
                  fx = (itp->first + kfbound);
                }
              }
              if (itn != data.end()) {
                if (fx > (itn->first - kfbound)) {
                  fx = (itn->first - kfbound);
                }
              }
              data_t::iterator it        = data.begin() + miPoint;
              std::pair<float, fvec4> pr = (*it);
              data.RemoveItem(it);
              data.AddSorted(fx, pr.second);
            }
          }
        }
        break;
      }
      case ui::EventCode::DOUBLECLICK: {

        if (_parent && mGradientObject) {
          data_t& data = mGradientObject->_data;

          bool is_left  = filtev.mBut0;
          bool is_right = filtev.mBut2;

          data_t::iterator it        = data.begin() + miPoint;
          std::pair<float, fvec4> pr = (*it);

          if (is_left) {

            bool bok = false;

            fvec4 inp = it->second;

            /*QRgb rgba = qRgba(inp.x * 255.0f, inp.y * 255.0f, inp.z * 255.0f, inp.w * 255.0f);

            rgba = QColorDialog::getRgba(rgba, &bok, 0);

            if (bok) {
              data.RemoveItem(it);
              int ir = qRed(rgba);
              int ig = qGreen(rgba);
              int ib = qBlue(rgba);
              int ia = qAlpha(rgba);
              fvec4 nc(float(ir) / 256.0f, float(ig) / 256.0f, float(ib) / 256.0f, float(ia) / 256.0f);
              data.AddSorted(pr.first, nc);
            }
          } else if (is_right) {
            if (it->first != 0.0f && it->first != 1.0f) {
              data.RemoveItem(it);
            }
          }*/
          }
          break;
        }
      }
    }
  }
};

class GedGradientEditSeg : public GedObject {
  DeclareAbstractX(GedGradientEditSeg, GedObject);

  gradient_fvec4_t* mGradientObject;
  GedItemNode* _parent;
  int miSeg;

public:
  void OnUiEvent(ork::ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    switch (filtev._eventcode) {
      case ui::EventCode::DOUBLECLICK: {
        // printf( "GradSplit par<%p> go<%p>\n", _parent, mGradientObject );
        if (_parent && mGradientObject) {
          orklut<float, ork::fvec4>& data = mGradientObject->_data;
          bool bok                        = false;

          std::pair<float, ork::fvec4> pointa = data.GetItemAtIndex(miSeg);
          std::pair<float, ork::fvec4> pointb = data.GetItemAtIndex(miSeg + 1);

          ork::fvec4 plerp = (pointa.second + pointb.second) * 0.5f;
          float filerp     = (pointa.first + pointb.first) * 0.5f;

          data.AddSorted(filerp, plerp);
        }
        break;
      }
    }
  }

  void SetSeg(int idx) {
    miSeg = idx;
  }

  GedGradientEditSeg()
      : mGradientObject(0)
      , _parent(0)
      , miSeg(-1) {
  }

  void SetGradientObject(gradient_fvec4_t* pgrad) {
    mGradientObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    _parent = ppar;
  }
};

void GedGradientEditSeg::describeX(class_t* clazz) {
}


////////////////////////////////////////////////////////////////

void GedGradientEditPoint::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

struct GradientEditorImpl {
  GradientEditorImpl(GedGradientNode* node)
      : _node(node) {
  }
  GedGradientNode* _node = nullptr;
};

////////////////////////////////////////////////////////////////

void GedGradientNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedGradientNode::GedGradientNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object) {

  auto gei = _impl.makeShared<GradientEditorImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedGradientNode::DoDraw(lev2::Context* pTARG) {
  auto model   = _container->_model;
  auto skin    = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if (not is_pick) {
    skin->DrawText(this, miX, miY, _propname.c_str());
  }
}

void GedGradientNode::OnUiEvent(ork::ui::event_constptr_t ev) {
}
int GedGradientNode::doComputeHeight() {
  return 100;
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryGradient::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

geditemnode_ptr_t
GedNodeFactoryGradient::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedGradientNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedGradientEditPoint, "GedGradientEditPoint");
ImplementReflectionX(ork::lev2::ged::GedGradientEditSeg, "GedGradientEditSeg");
ImplementReflectionX(ork::lev2::ged::GedGradientNode, "GedGradientNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryGradient, "GedNodeFactoryGradient");
