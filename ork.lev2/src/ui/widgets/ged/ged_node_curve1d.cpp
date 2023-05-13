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
#include <ork/math/multicurve.h>
#include <ork/kernel/orkpool.h>
#include <ork/reflect/properties/registerX.inl>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
static constexpr int kpntsize = 5;
static constexpr int kh       = 128;
////////////////////////////////////////////////////////////////

struct GedCurve1DEditPoint : public GedObject {
  DeclareAbstractX(GedCurve1DEditPoint, GedObject);
  MultiCurve1D* mCurveObject;
  GedItemNode* _parent;
  int miPoint;

public:
  void SetPoint(int idx) {
    miPoint = idx;
  }
  GedCurve1DEditPoint()
      : mCurveObject(0)
      , _parent(0)
      , miPoint(-1) {
  }
  void SetCurveObject(MultiCurve1D* pgrad) {
    mCurveObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    _parent = ppar;
  }
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final {
    if (_parent && mCurveObject) {
      orklut<float, float>& data        = mCurveObject->GetVertices();
      orklut<float, float>::iterator it = data.begin() + miPoint;
      if (ev->IsButton0DownF()) {
        bool bok = false;
      } else if (ev->IsButton2DownF()) {
        if (it->first != 0.0f && it->first != 1.0f) {
          mCurveObject->MergeSegment(miPoint);
          //_parent->SigInvalidateProperty();
        }
      }
    }
  }
  void OnUiEvent(ork::ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    switch (filtev._eventcode) {
      case ui::EventCode::DRAG: {
        if (_parent && mCurveObject) {
          orklut<float, float>& data = mCurveObject->GetVertices();
          const int knumpoints       = (int)data.size();
          const int ksegs            = knumpoints - 1;
          if (miPoint >= 0 && miPoint < knumpoints) {
            int mouseposX = ev->miX - _parent->GetX();
            int mouseposY = ev->miY - _parent->GetY();
            float fx      = float(mouseposX) / float(_parent->width());
            float fy      = 1.0f - float(mouseposY) / float(kh);
            if (miPoint == 0)
              fx = 0.0f;
            if (miPoint == (knumpoints - 1))
              fx = 1.0f;
            if (fy < 0.0f)
              fy = 0.0f;
            if (fy > 1.0f)
              fy = 1.0f;

            orklut<float, float>::iterator it = data.begin() + miPoint;

            if (miPoint != 0 && miPoint != (knumpoints - 1)) {
              orklut<float, float>::iterator itp = it - 1;
              orklut<float, float>::iterator itn = it + 1;
              const float kfbound                = float(kpntsize) / _parent->width();
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
            }
            data.RemoveItem(it);
            data.AddSorted(fx, fy);
            //_parent->SigInvalidateProperty();
          }
        }
        break;
      }
      default:
        break;
    }
  }
};

void GedCurve1DEditPoint::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

struct GedCurve1DEditSeg : public GedObject {
  DeclareAbstractX(GedCurve1DEditSeg, GedObject);

public:
  ///////////////////////////////////
  GedCurve1DEditSeg()
      : mCurveObject(0)
      , _parent(0)
      , miSeg(-1) {
  }
  ///////////////////////////////////
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final {
    if (_parent && mCurveObject) {
      if (ev->IsButton0DownF()) {
        mCurveObject->SplitSegment(miSeg);
        //_parent->SigInvalidateProperty();
      } else if (ev->IsButton2DownF()) {
        int sx = ev->miScreenPosX;
        int sy = ev->miScreenPosY; // - H*2;
        std::vector<std::string> choices;
        choices.push_back("Seg:Lin");
        choices.push_back("Seg:Box");
        choices.push_back("Seg:Log");
        choices.push_back("Seg:Exp");
        std::string choice = ui::popupChoiceList(
            _parent->_l2context(), //
            sx,
            sy,
            choices);
        if (choice == "Seg:Lin")
          mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::LINEAR);
        if (choice == "Seg:Box")
          mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::BOX);
        if (choice == "Seg:Log")
          mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::LOG);
        if (choice == "Seg:Exp")
          mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::EXP);
        //_parent->SigInvalidateProperty();
      }
    }
  }

  void SetSeg(int idx) {
    miSeg = idx;
  }

  void SetCurveObject(MultiCurve1D* pgrad) {
    mCurveObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    _parent = ppar;
  }
  MultiCurve1D* mCurveObject;
  GedItemNode* _parent;
  int miSeg;
};

////////////////////////////////////////////////////////////////

void GedCurve1DEditSeg::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

struct CurveEditorImpl {

  static const int kpoolsize = 32;

  CurveEditorImpl(GedCurve1DNode* node)
      : _node(node)
      , mEditPoints(kpoolsize)
      , mEditSegs(kpoolsize) {

    mCurveObject = dynamic_cast<MultiCurve1D*>(node->_iodriver->_object.get());
    OrkAssert(mCurveObject);

    static int ginstid = 0;
    _instanceID        = ginstid++;
  }
  void render(lev2::Context* pTARG);

  GedCurve1DNode* _node      = nullptr;
  MultiCurve1D* mCurveObject = nullptr;
  ork::pool<GedCurve1DEditPoint> mEditPoints;
  ork::pool<GedCurve1DEditSeg> mEditSegs;
  int _instanceID = 0;

};

using curveeditorimpl_ptr_t = std::shared_ptr<CurveEditorImpl>;

////////////////////////////////////////////////////////////////

void CurveEditorImpl::render(lev2::Context* pTARG) {

  auto container = _node->_container;
  auto model     = container->_model;
  auto skin      = container->_activeSkin;
  bool is_pick   = skin->_is_pickmode;

  int x = _node->miX;
  int y = _node->miY;
  int w = _node->miW;
  int h = _node->miH;

  const orklut<float, float>& data = mCurveObject->GetVertices();

  skin->pushCustomColor(fcolor3::Blue());
  skin->DrawBgBox(_node, x + 1, y + 2, w - 3, h - 3, GedSkin::ESTYLE_CUSTOM_COLOR, 0);
  skin->popCustomColor();

  ////////////////////////////////////

  const int knumpoints = (int)data.size();
  const int ksegs      = knumpoints - 1;

  ////////////////////////////////////
  // draw segments (for picking)
  ////////////////////////////////////

  if (pTARG->FBI()->isPickState()) {
    mEditSegs.clear();

    for (int i = 0; i < ksegs; i++) {
      auto pointa = data.GetItemAtIndex(i);
      auto pointb = data.GetItemAtIndex(i + 1);

      auto editseg = mEditSegs.allocate();
      editseg->SetCurveObject(mCurveObject);
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

  ////////////////////////////////////////////////////////////////////////
  // line segments
  ////////////////////////////////////////////////////////////////////////

  static const int kexplogsegs = 16;
  int inuml                    = 0;
  for (int i = 0; i < ksegs; i++) {
    switch (mCurveObject->GetSegmentType(i)) {
      case MultiCurveSegmentType::LINEAR:
        inuml += 2;
        break;
      case MultiCurveSegmentType::BOX:
        inuml += 4;
        break;
      case MultiCurveSegmentType::LOG:
        inuml += kexplogsegs * 2;
        break;
      case MultiCurveSegmentType::EXP:
        inuml += kexplogsegs * 2;
        break;
    }
  }
  float fmin = mCurveObject->GetMin();
  float fmax = mCurveObject->GetMax();
  float frng = (fmax - fmin);

  for (int i = 0; i < ksegs; i++) {
    std::pair<float, float> data_a = data.GetItemAtIndex(i);
    std::pair<float, float> data_b = data.GetItemAtIndex(i + 1);

    float fia  = data_a.first;
    float fib  = data_b.first;
    float fiya = data_a.second;
    float fiyb = data_b.second;

    float fx0 = x + (fia * w);
    float fx1 = x + (fib * w);
    float fy0 = y+h - (fiya * h);
    float fy1 = y+h - (fiyb * h);

    skin->pushCustomColor(fcolor3::White());

    switch (mCurveObject->GetSegmentType(i)) {
      case MultiCurveSegmentType::LOG:
      case MultiCurveSegmentType::EXP: {
        for (int j = 0; j < kexplogsegs; j++) {
          int k      = j + 1;
          float fj   = float(j) / float(kexplogsegs);
          float fk   = float(k) / float(kexplogsegs);
          float fiaL = (fj * fib) + (1.0f - fj) * fia;
          float fiaR = (fk * fib) + (1.0f - fk) * fia;
          float fsL  = mCurveObject->Sample(fiaL);
          float fsR  = mCurveObject->Sample(fiaR);

          float fuL = (fsL - fmin) / frng;
          float fuR = (fsR - fmin) / frng;

          fx0 = x + (fiaL * w);
          fx1 = x + (fiaR * w);
          fy0 = y+h - (fuL * h);
          fy1 = y+h - (fuR * h);

          skin->DrawLine(_node, fx0, fy0, fx1, fy1, GedSkin::ESTYLE_CUSTOM_COLOR);
        }
        break;
      }
      case MultiCurveSegmentType::LINEAR: {
        skin->DrawLine(_node, fx0, fy0, fx1, fy1, GedSkin::ESTYLE_CUSTOM_COLOR);
        break;
      }
      case MultiCurveSegmentType::BOX: {
        skin->DrawLine(_node, fx0, fy0, fx1, fy0, GedSkin::ESTYLE_CUSTOM_COLOR);
        skin->DrawLine(_node, fx1, fy0, fx1, fy1, GedSkin::ESTYLE_CUSTOM_COLOR);
        break;
      }
    }
  }

  skin->popCustomColor();
  
  ////////////////////////////////////
  // draw points
  ////////////////////////////////////

  mEditPoints.clear();

  for (int i = 0; i < knumpoints; i++) {
    std::pair<float, float> point = data.GetItemAtIndex(i);

    auto editpoint = mEditPoints.allocate();
    editpoint->SetCurveObject(mCurveObject);
    editpoint->SetParent(_node);
    editpoint->SetPoint(i);

    int ixc = int(point.first * float(w));
    int iyc = int(point.second * float(h));
    int fx0 = x + (ixc - kpntsize);
    int fy0 = y + (h - (kpntsize)) - iyc;

    bool is_highligted = _node->IsObjectHilighted(editpoint);

    if (pTARG->FBI()->isPickState()) {
      skin->DrawBgBox(editpoint, fx0, fy0, kpntsize * 2, kpntsize * 2, GedSkin::ESTYLE_DEFAULT_CHECKBOX, 2);
    } else {
      int pntsizex2 = kpntsize * 2;
      // outer
      skin->pushCustomColor(is_highligted ? fcolor3::White() : fcolor3::White());
      skin->DrawOutlineBox(editpoint, fx0 + 1, //
                                      fy0 + 1, //
                                      pntsizex2 - 2, //
                                      pntsizex2 - 2, //
                                      GedSkin::ESTYLE_CUSTOM_COLOR, 3);
      skin->popCustomColor();
      // inner
      skin->pushCustomColor(fcolor3::Black());
      skin->DrawOutlineBox(editpoint, fx0, //
                                      fy0, //
                                      pntsizex2, //
                                      pntsizex2, //
                                      GedSkin::ESTYLE_CUSTOM_COLOR, 2);
      skin->popCustomColor();
    }
  }
}

////////////////////////////////////////////////////////////////

void GedCurve1DNode::describeX(class_t* clazz) {
}

GedCurve1DNode::GedCurve1DNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object)
    , _iodriver(iodriver) {

  auto cei = _impl.makeShared<CurveEditorImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedCurve1DNode::DoDraw(lev2::Context* pTARG) { // final
  auto cei = _impl.getShared<CurveEditorImpl>();
  cei->render(pTARG);
}

int GedCurve1DNode::doComputeHeight() { // final
  return kh;
}

////////////////////////////////////////////////////////////////

void GedCurve1DNode::OnUiEvent(ork::ui::event_constptr_t ev) {
  return GedItemNode::OnUiEvent(ev);
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryCurve1D::describeX(class_t* clazz) {
}

geditemnode_ptr_t
GedNodeFactoryCurve1D::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedCurve1DNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedCurve1DNode, "GedCurve1DNode");
ImplementReflectionX(ork::lev2::ged::GedCurve1DEditPoint, "GedCurve1DEditPoint");
ImplementReflectionX(ork::lev2::ged::GedCurve1DEditSeg, "GedCurve1DEditSeg");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryCurve1D, "GedNodeFactoryCurve1D");
