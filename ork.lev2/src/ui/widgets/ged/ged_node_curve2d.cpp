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

struct GedCurve2DEditPoint : public GedObject {
  DeclareAbstractX(GedCurve2DEditPoint, GedObject);
  MultiCurve1D* mCurveObject;
  GedItemNode* _parent;
  int miPoint;

public:
  void SetPoint(int idx) {
    miPoint = idx;
  }
  GedCurve2DEditPoint()
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

void GedCurve2DEditPoint::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

struct GedCurve2DEditSeg : public GedObject {
  DeclareAbstractX(GedCurve2DEditSeg, GedObject);
public:
  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final {
    if (_parent && mCurveObject) {
      if (ev->IsButton0DownF()) {
        mCurveObject->SplitSegment(miSeg);
        //_parent->SigInvalidateProperty();
      } else if (ev->IsButton2DownF()) {
        /*
        QMenu* pMenu         = new QMenu(0);
        QAction* pchildmenu0 = pMenu->addAction("Seg:Lin");
        QAction* pchildmenu1 = pMenu->addAction("Seg:Box");
        QAction* pchildmenu2 = pMenu->addAction("Seg:Log");
        QAction* pchildmenu3 = pMenu->addAction("Seg:Exp");

        pchildmenu0->setData(QVariant("lin"));
        pchildmenu1->setData(QVariant("box"));
        pchildmenu2->setData(QVariant("log"));
        pchildmenu3->setData(QVariant("exp"));

        QAction* pact = pMenu->exec(QCursor::pos());

        if (pact) {
          QVariant UserData = pact->data();
          QString UserName  = UserData.toString();
          std::string sval  = UserName.toStdString();
          if (sval == "lin")
            mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::LINEAR);
          if (sval == "box")
            mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::BOX);
          if (sval == "log")
            mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::LOG);
          if (sval == "exp")
            mCurveObject->SetSegmentType(miSeg, MultiCurveSegmentType::EXP);
          _parent->SigInvalidateProperty();
        }*/
      }
    }
  }

  void SetSeg(int idx) {
    miSeg = idx;
  }

  GedCurve2DEditSeg()
      : mCurveObject(0)
      , _parent(0)
      , miSeg(-1) {
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

void GedCurve2DEditSeg::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

struct CurveEditorImpl {

  static const int kpoolsize = 32;

  CurveEditorImpl(GedCurve2DNode* node)
      : _node(node)
      , mEditPoints(kpoolsize)
      , mEditSegs(kpoolsize) {
    //if (0 == mCurveObject) {
      //const reflect::IObject* pprop = rtti::autocast(GetOrkProp());
      //mCurveObject                  = rtti::autocast(pprop->Access(GetOrkObj()));
   // }

    //if (0 == mCurveObject) {
      //const reflect::IObject* pprop = rtti::autocast(GetOrkProp());
      //ObjProxy<MultiCurve1D>* proxy = rtti::autocast(pprop->Access(GetOrkObj()));
      //mCurveObject                  = proxy->_parent;
    //}
  }
  void render(lev2::Context* pTARG);

  GedCurve2DNode* _node      = nullptr;
  MultiCurve1D* mCurveObject = nullptr;
  ork::pool<GedCurve2DEditPoint> mEditPoints;
  ork::pool<GedCurve2DEditSeg> mEditSegs;
  // ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>	mVertexBuffer;
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

  skin->DrawBgBox(_node, x, y, w, h, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if (not is_pick) {
    skin->DrawText(_node, x, y, _node->_propname.c_str());
  }
}

////////////////////////////////////////////////////////////////

void GedCurve2DNode::describeX(class_t* clazz) {
}

GedCurve2DNode::GedCurve2DNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object)
    , _iodriver(iodriver) {

  auto cei = _impl.makeShared<CurveEditorImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedCurve2DNode::DoDraw(lev2::Context* pTARG) { // final
  auto cei = _impl.getShared<CurveEditorImpl>();
  cei->render(pTARG);
}

int GedCurve2DNode::computeHeight() { // final
  return kh;
} 


////////////////////////////////////////////////////////////////

void GedCurve2DNode::OnUiEvent(ork::ui::event_constptr_t ev) {
  return GedItemNode::OnUiEvent(ev);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedCurve2DNode, "GedCurve2DNode");
ImplementReflectionX(ork::lev2::ged::GedCurve2DEditPoint, "GedCurve2DEditPoint");
ImplementReflectionX(ork::lev2::ged::GedCurve2DEditSeg, "GedCurve2DEditSeg");
