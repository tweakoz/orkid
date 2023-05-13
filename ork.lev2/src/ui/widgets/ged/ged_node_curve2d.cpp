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
  ///////////////////////////////////
  GedCurve2DEditSeg()
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

void GedCurve2DEditSeg::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

struct CurveEditorImpl {

  static const int kpoolsize = 32;

  CurveEditorImpl(GedCurve2DNode* node)
      : _node(node)
      , mEditPoints(kpoolsize)
      , mEditSegs(kpoolsize) {

        mCurveObject = dynamic_cast<MultiCurve1D*>(node->_iodriver->_object.get());
        OrkAssert(mCurveObject);
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

static void CurveCustomPrim(
    GedSkin* pskin,              //
    GedObject* pnode,            //
    ork::lev2::Context* pTARG) { //

  auto typed_node          = dynamic_cast<GedCurve2DNode*>(pnode);
  auto pthis = typed_node->_impl.getShared<CurveEditorImpl>();
  const orklut<float, float>& data = pthis->mCurveObject->GetVertices();
  const int knumpoints             = (int)data.size();
  const int ksegs                  = knumpoints - 1;

  int x = typed_node->miX;
  int y = typed_node->miY;
  int w = typed_node->miW;
  int h = typed_node->miH;

  if (0 == ksegs)
    return;

  if (pTARG->FBI()->isPickState()) {
  } else if (pthis->mCurveObject) {
    lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();
    lev2::GfxMaterial3DSolid gridmat(pTARG);
    gridmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR);
    gridmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    gridmat._rasterstate.SetCullTest(ork::lev2::ECullTest::OFF);
    gridmat._rasterstate.SetBlending(ork::lev2::Blending::OFF);
    gridmat._rasterstate.SetDepthTest(ork::lev2::EDepthTest::ALWAYS);
    gridmat._rasterstate.SetShadeModel(ork::lev2::ESHADEMODEL_SMOOTH);

    // pthis->mVertexBuffer.Reset();

    static const int kexplogsegs = 16;
    int inuml                    = 0;
    for (int i = 0; i < ksegs; i++) {
      switch (pthis->mCurveObject->GetSegmentType(i)) {
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

    int ivbaseA  = VB.GetNumVertices();
    int reserveA = 1024;

    lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
    vw.Lock(pTARG, &VB, reserveA);

    fvec2 uv;

    int icountA = 0;

    const float kz = 0.0f;

    float fx = float(x);
    float fy = float(y + pskin->_scrollY);
    float fw = float(w);
    float fh = float(kh);

    lev2::SVtxV12C4T16 v0(fvec3(fx, fy, kz), uv, 0xffffffff);
    lev2::SVtxV12C4T16 v1(fvec3(fx + fw, fy, kz), uv, 0xffffffff);
    lev2::SVtxV12C4T16 v2(fvec3(fx + fw, fy + fh, kz), uv, 0xffffffff);
    lev2::SVtxV12C4T16 v3(fvec3(fx, fy + fh, kz), uv, 0xffffffff);

    vw.AddVertex(v0);
    vw.AddVertex(v1);
    vw.AddVertex(v2);

    vw.AddVertex(v0);
    vw.AddVertex(v2);
    vw.AddVertex(v3);

    icountA += 6;

    float fmin = pthis->mCurveObject->GetMin();
    float fmax = pthis->mCurveObject->GetMax();
    float frng = (fmax - fmin);

    for (int i = 0; i < ksegs; i++) {
      std::pair<float, float> data_a = data.GetItemAtIndex(i);
      std::pair<float, float> data_b = data.GetItemAtIndex(i + 1);

      float fia  = data_a.first;
      float fib  = data_b.first;
      float fiya = data_a.second;
      float fiyb = data_b.second;

      float fx0 = fx + (fia * fw);
      float fx1 = fx + (fib * fw);
      float fy0 = fy + fh - (fiya * fh);
      float fy1 = fy + fh - (fiyb * fh);

      switch (pthis->mCurveObject->GetSegmentType(i)) {
        case MultiCurveSegmentType::LOG:
        case MultiCurveSegmentType::EXP: {
          for (int j = 0; j < kexplogsegs; j++) {
            int k      = j + 1;
            float fj   = float(j) / float(kexplogsegs);
            float fk   = float(k) / float(kexplogsegs);
            float fiaL = (fj * fib) + (1.0f - fj) * fia;
            float fiaR = (fk * fib) + (1.0f - fk) * fia;
            float fsL  = pthis->mCurveObject->Sample(fiaL);
            float fsR  = pthis->mCurveObject->Sample(fiaR);

            float fuL = (fsL - fmin) / frng;
            float fuR = (fsR - fmin) / frng;

            fx0 = fx + (fiaL * fw);
            fx1 = fx + (fiaR * fw);
            fy0 = fy + fh - (fuL * fh);
            fy1 = fy + fh - (fuR * fh);
            lev2::SVtxV12C4T16 v0(fvec3(fx0, fy0, kz), uv, 0xffffffff);
            lev2::SVtxV12C4T16 v1(fvec3(fx1, fy1, kz), uv, 0xffffffff);
            vw.AddVertex(v0);
            vw.AddVertex(v1);
            icountA += 2;
          }
          break;
        }
        case MultiCurveSegmentType::LINEAR: {
          lev2::SVtxV12C4T16 v0(fvec3(fx0, fy0, kz), uv, 0xffffffff);
          lev2::SVtxV12C4T16 v1(fvec3(fx1, fy1, kz), uv, 0xffffffff);
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          icountA += 2;
          break;
        }
        case MultiCurveSegmentType::BOX: {
          lev2::SVtxV12C4T16 v0(fvec3(fx0, fy0, kz), uv, 0xffffffff);
          lev2::SVtxV12C4T16 v1(fvec3(fx1, fy0, kz), uv, 0xffffffff);
          lev2::SVtxV12C4T16 v2(fvec3(fx1, fy1, kz), uv, 0xffffffff);
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v1);
          vw.AddVertex(v2);
          icountA += 4;
          break;
        }
      }
    }
    vw.UnLock(pTARG);

    ////////////////////////////////////////////////////////////////
    F32 fVPW = (F32)pTARG->FBI()->GetVPW();
    F32 fVPH = (F32)pTARG->FBI()->GetVPH();
    if (0.0f == fVPW)
      fVPW = 1.0f;
    if (0.0f == fVPH)
      fVPH = 1.0f;
    fmtx4 mtxortho = pTARG->MTXI()->Ortho(0.0f, fVPW, 0.0f, fVPH, 0.0f, 1.0f);
    pTARG->MTXI()->PushPMatrix(mtxortho);
    pTARG->MTXI()->PushVMatrix(fmtx4::Identity());
    pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
    pTARG->PushModColor(fvec3::Blue());
    pTARG->GBI()->DrawPrimitive(&gridmat, VB, ork::lev2::PrimitiveType::TRIANGLES, ivbaseA, 6);
    pTARG->PopModColor();
    pTARG->PushModColor(fvec3::White());
    pTARG->GBI()->DrawPrimitive(&gridmat, VB, ork::lev2::PrimitiveType::LINES, ivbaseA + 6, icountA - 6);
    pTARG->PopModColor();
    pTARG->MTXI()->PopPMatrix();
    pTARG->MTXI()->PopVMatrix();
    pTARG->MTXI()->PopMMatrix();
    ////////////////////////////////////////////////////////////////

  } else {
    OrkAssert(false);
  }
}

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

  skin->DrawBgBox(_node, x, y + 2, w, kh - 3, GedSkin::ESTYLE_BACKGROUND_1);
  GedSkin::GedPrim prim;
  prim.mDrawCB = CurveCustomPrim;
  prim.mpNode  = _node;
  prim.meType  = ork::lev2::PrimitiveType::MULTI;
  prim.iy1     = y;
  prim.iy2     = y + kh;
  skin->AddPrim(prim);

  ////////////////////////////////////

  const int knumpoints = (int)data.size();
  const int ksegs      = knumpoints - 1;

  ////////////////////////////////////
  // draw segments

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
      skin->DrawOutlineBox(editseg, fx0, y, fw0, kh, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 1);
    }
  }

  ////////////////////////////////////
  // draw points

  mEditPoints.clear();

  for (int i = 0; i < knumpoints; i++) {
    std::pair<float, float> point = data.GetItemAtIndex(i);

    auto editpoint = mEditPoints.allocate();
    editpoint->SetCurveObject(mCurveObject);
    editpoint->SetParent(_node);
    editpoint->SetPoint(i);

    int fxc = int(point.first * float(w));
    int fyc = int(point.second * float(kh));
    int fx0 = x + (fxc - kpntsize);
    int fy0 = y + (kh - (kpntsize)) - fyc;

    GedSkin::ESTYLE pntstyl =
        _node->IsObjectHilighted(editpoint) ? GedSkin::ESTYLE_DEFAULT_HIGHLIGHT : GedSkin::ESTYLE_DEFAULT_CHECKBOX;
    if (pTARG->FBI()->isPickState()) {
      skin->DrawBgBox(editpoint, fx0, fy0, kpntsize * 2, kpntsize * 2, pntstyl, 2);
    } else {
      skin->DrawOutlineBox(editpoint, fx0 + 1, fy0 + 1, kpntsize * 2 - 2, kpntsize * 2 - 2, pntstyl, 2);
    }
    skin->DrawOutlineBox(editpoint, fx0, fy0, kpntsize * 2, kpntsize * 2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 2);
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

void GedNodeFactoryCurve1D::describeX(class_t* clazz) {
}

geditemnode_ptr_t GedNodeFactoryCurve1D::createItemNode( GedContainer* container, 
                                                          const ConstString& Name, 
                                                          newiodriver_ptr_t iodriver ) const {
  auto node =  std::make_shared<GedCurve2DNode>(container, Name.c_str(), iodriver);
  return node;
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedCurve2DNode, "GedCurve2DNode");
ImplementReflectionX(ork::lev2::ged::GedCurve2DEditPoint, "GedCurve2DEditPoint");
ImplementReflectionX(ork::lev2::ged::GedCurve2DEditSeg, "GedCurve2DEditSeg");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryCurve1D, "GedNodeFactoryCurve1D");
