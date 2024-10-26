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

namespace ork::lev2{
  void gradientGeometry(
    Context* pTARG, //
    const ork::gradient_fvec4_t& grad, //
    VtxWriter<SVtxV16T16C16>& vw,
    int x1, //
    int y1, //
    int w, //
    int h);
}
////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
static constexpr int kpntsize = 5;
static constexpr int kh       = 128;
////////////////////////////////////////////////////////////////

class GedGradientEditPoint : public GedObject {
  DeclareAbstractX(GedGradientEditPoint, GedObject);

  gradient_fvec4_t* mGradientObject = nullptr;
  GedItemNode* _parent              = nullptr;
  int miPoint                       = -1;

public:
  void SetPoint(int idx) {
    miPoint = idx;
  }

  void SetGradientObject(gradient_fvec4_t* pgrad) {
    mGradientObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    _parent = ppar;
  }

  bool OnUiEvent(ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    using data_t = orklut<float, fvec4>;

    switch (filtev._eventcode) {
      case ui::EventCode::DRAG: {
        if (_parent && mGradientObject) {
          data_t& data         = mGradientObject->_data;
          const int knumpoints = (int)data.size();
          const int ksegs      = knumpoints - 1;

          if (miPoint == 0 or miPoint == (knumpoints - 1)) {
            int mposy   = ev->miY - _parent->GetY();
            float fy     = float(mposy) / float(_parent->height());
            data_t::iterator it  = data.begin() + miPoint;
            std::pair<float, fvec4> pr = (*it);
            fvec4 color = pr.second;
            color.w = std::clamp(fy, 0.0f, 1.0f);
            data.RemoveItem(it);
            data.AddSorted(pr.first, color);
          }
          else if (miPoint > 0 and miPoint < (knumpoints - 1)) {
            int mousepos = ev->miX - _parent->GetX();
            int mposy   = ev->miY - _parent->GetY();
            float fx     = float(mousepos) / float(_parent->width());
            float fy     = float(mposy) / float(_parent->height());

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

              fvec4 color = pr.second;
              color.w = std::clamp(fy, 0.0f, 1.0f);

              data.RemoveItem(it);
              data.AddSorted(fx, color);
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

            fvec4 initial_color = it->second;
            
            const int kdim  = 256;
            int sx = ev->miScreenPosX - (kdim>>1);
            int sy = ev->miScreenPosY - (kdim>>1);
            if( sx<0)
                sx = 0;
            if( sy<0)
                sy = 0;
                
            int W = kdim;
            int H = kdim;
            auto ctx = _parent->_l2context();

            fvec4 selected = ui::popupColorEdit(ctx, sx, sy, W, H, initial_color);

            data.RemoveItem(it);
            data.AddSorted(pr.first, selected);

          } else if (is_right) {
            if (it->first != 0.0f && it->first != 1.0f) {
              data.RemoveItem(it);
            }
          }
          break;
        }
      }
      default:
        break;
    }
    return true;
  }
};

class GedGradientEditSeg : public GedObject {
  DeclareAbstractX(GedGradientEditSeg, GedObject);

  gradient_fvec4_t* mGradientObject;
  GedItemNode* _parent;
  int miSeg;

public:
  bool OnUiEvent(ork::ui::event_constptr_t ev) final {
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
      default:
        break;
    }
    return true;
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

  static const int kpoolsize = 32;

  GradientEditorImpl(GedGradientNode* node)
      : _node(node)
      , mEditPoints(kpoolsize)
      , mEditSegs(kpoolsize) {

    _node     = node;
    _iodriver = node->_iodriver;
    OrkAssert(_iodriver);
    _gradient = dynamic_cast<gradient_fvec4_t*>(_iodriver->_object.get());
    OrkAssert(_gradient);
  }
  void render(lev2::Context* pTARG);
  GedGradientNode* _node      = nullptr;
  gradient_fvec4_t* _gradient = nullptr;
  newiodriver_ptr_t _iodriver;
  ork::pool<GedGradientEditPoint> mEditPoints;
  ork::pool<GedGradientEditSeg> mEditSegs;
};

////////////////////////////////////////////////////////////////

void GradientEditorImpl::render(lev2::Context* pTARG) {
  auto container = _node->_container;
  auto model     = container->_model;
  auto skin      = container->_activeSkin;
  bool is_pick   = skin->_is_pickmode;

  int x = _node->miX;
  int y = _node->miY;
  int w = _node->miW;
  int h = _node->miH;

  ////////////////////////////////////

  const auto& data     = _gradient->_data;
  const int knumpoints = (int)data.size();
  const int ksegs      = knumpoints - 1;

  ////////////////////////////////////
  // draw gradient itself
  ////////////////////////////////////

  if (ksegs and not is_pick) {

    GedSkin::GedPrim custom_prim;
    custom_prim.mpNode        = _node;
    custom_prim.iy1           = y - skin->_scrollY;
    custom_prim.iy2           = y - skin->_scrollY + h;
    custom_prim._renderLambda = [=]() {
      ///////////////////////////////
      VtxWriter<SVtxV16T16C16> vw;
      gradientGeometry( //
          pTARG,        //
          *_gradient,   //
          vw,
          x,           //
          y, //
          w,           //
          h);
      ///////////////////////////////
      float time = skin->_timer.SecsSinceStart();
      auto RCFD = std::make_shared<RenderContextFrameData>(pTARG);
      skin->_material->_rasterstate.SetRGBAWriteMask(true, true);
      skin->_material->begin(skin->_tekcolorwheel, RCFD);
      skin->_material->bindParamMatrix(skin->_parmvp, skin->_uiMVPMatrix);
      skin->_material->bindParamFloat(skin->_partime, time);
      pTARG->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
      skin->_material->end(RCFD);
      ///////////////////////////////
    };
    skin->AddPrim(custom_prim);
  }
  ////////////////////////////////////
  // draw segments (for picking)
  ////////////////////////////////////

  if (pTARG->FBI()->isPickState()) {
    mEditSegs.clear();

    for (int i = 0; i < ksegs; i++) {
      auto pointa = data.GetItemAtIndex(i);
      auto pointb = data.GetItemAtIndex(i + 1);

      auto editseg = mEditSegs.allocate();
      editseg->SetGradientObject(_gradient);
      editseg->SetParent(_node);
      editseg->SetSeg(i);

      float fi0 = pointa.first;
      float fi1 = pointb.first;
      float fw  = (fi1 - fi0);

      int fx0 = x + int(fi0 * float(w));
      int fw0 = int(fw * float(w));

      skin->DrawBgBox(editseg, fx0, y, fw0, kh, GedSkin::ESTYLE_DEFAULT_CHECKBOX, 1);
      // skin->DrawOutlineBox(editseg, fx0, y-1, fw0, kh-2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 1);
    }
  }

  ////////////////////////////////////
  // draw points
  ////////////////////////////////////

  mEditPoints.clear();

  for (int i = 0; i < knumpoints; i++) {
    auto point = data.GetItemAtIndex(i);

    auto editpoint = mEditPoints.allocate();
    editpoint->SetGradientObject(_gradient);
    editpoint->SetParent(_node);
    editpoint->SetPoint(i);

    int fxc = int(point.first * float(w));
    int fx0 = x + (fxc - kpntsize);
    int fy0 = y + (kh - (kpntsize * 2));

    GedSkin::ESTYLE pntstyl =
        _node->IsObjectHilighted(editpoint) ? GedSkin::ESTYLE_DEFAULT_HIGHLIGHT : GedSkin::ESTYLE_DEFAULT_CHECKBOX;

    if (pTARG->FBI()->isPickState()) {
      skin->DrawBgBox(editpoint, fx0 + 1, fy0 + 1, kpntsize * 2 - 2, kpntsize * 2 - 2, pntstyl, 2);
    } else {
      skin->DrawOutlineBox(editpoint, fx0 + 1, fy0 + 1, kpntsize * 2 - 2, kpntsize * 2 - 2, pntstyl, 2);
    }
    skin->DrawOutlineBox(editpoint, fx0, fy0, kpntsize * 2, kpntsize * 2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 2);
  }
}

////////////////////////////////////////////////////////////////

void GedGradientNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedGradientNode::GedGradientNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object){
  _iodriver = iodriver;

  auto gei = _impl.makeShared<GradientEditorImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedGradientNode::DoDraw(lev2::Context* pTARG) {
  auto gei = _impl.getShared<GradientEditorImpl>();
  gei->render(pTARG);
}

////////////////////////////////////////////////////////////////

bool GedGradientNode::OnUiEvent(ork::ui::event_constptr_t ev) {
  switch(ev->_eventcode){
    case ui::EventCode::MOVE:
      _container->_viewport->MarkSurfaceDirty();
      _container->SlotRepaint();
      break;
    default:
      break;
  }
  return GedItemNode::OnUiEvent(ev);
}

////////////////////////////////////////////////////////////////

int GedGradientNode::doComputeHeight() const {
  return kh;
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryGradient::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

geditemnode_ptr_t
GedNodeFactoryGradient::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  OrkAssert(iodriver);
  return std::make_shared<GedGradientNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedGradientEditPoint, "GedGradientEditPoint");
ImplementReflectionX(ork::lev2::ged::GedGradientEditSeg, "GedGradientEditSeg");
ImplementReflectionX(ork::lev2::ged::GedGradientNode, "GedGradientNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryGradient, "GedNodeFactoryGradient");
