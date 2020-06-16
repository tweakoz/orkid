////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IObjectPropertyType.h>
#include "ged_delegate.hpp"
#include <ork/math/gradient.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <QtWidgets/QColorDialog>
#include <ork/kernel/orkpool.h>
#include <ork/lev2/ui/event.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

static const int kpntsize = 5;

class GedGradientEditPoint : public GedObject {
  RttiDeclareAbstract(GedGradientEditPoint, GedObject);

  ork::Gradient<ork::fvec4>* mGradientObject;
  GedItemNode* mParent;
  int miPoint;

public:
  void SetPoint(int idx) {
    miPoint = idx;
  }

  GedGradientEditPoint()
      : mGradientObject(0)
      , mParent(0)
      , miPoint(-1) {
  }

  void SetGradientObject(ork::Gradient<ork::fvec4>* pgrad) {
    mGradientObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    mParent = ppar;
  }

  void OnUiEvent(ork::ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    switch (filtev._eventcode) {
      case ui::EventCode::DRAG: {
        if (mParent && mGradientObject) {
          orklut<float, ork::fvec4>& data = mGradientObject->Data();
          const int knumpoints            = (int)data.size();
          const int ksegs                 = knumpoints - 1;

          if (miPoint > 0 && miPoint < (knumpoints - 1)) {
            int mousepos = ev->miX - mParent->GetX();
            float fx     = float(mousepos) / float(mParent->width());

            orklut<float, ork::fvec4>::iterator it  = data.begin() + miPoint;
            orklut<float, ork::fvec4>::iterator itp = it - 1;
            orklut<float, ork::fvec4>::iterator itn = it + 1;

            if (fx > 0.0f && fx < 1.0f) {
              const float kfbound = float(kpntsize) / mParent->width();
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
              orklut<float, ork::fvec4>::iterator it = data.begin() + miPoint;
              std::pair<float, ork::fvec4> pr        = (*it);
              data.RemoveItem(it);
              data.AddSorted(fx, pr.second);
            }
          }
        }
        break;
      }
      case ui::EventCode::DOUBLECLICK: {

        if (mParent && mGradientObject) {
          orklut<float, ork::fvec4>& data = mGradientObject->Data();

          bool is_left  = filtev.mBut0;
          bool is_right = filtev.mBut2;

          orklut<float, ork::fvec4>::iterator it = data.begin() + miPoint;
          std::pair<float, ork::fvec4> pr        = (*it);

          if (is_left) {

            bool bok = false;

            fvec4 inp = it->second;

            QRgb rgba = qRgba(inp.x * 255.0f, inp.y * 255.0f, inp.z * 255.0f, inp.w * 255.0f);

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
          }
        }
        break;
      }
    }
  }
};

void GedGradientEditPoint::Describe() {
}

class GedGradientEditSeg : public GedObject {
  RttiDeclareAbstract(GedGradientEditSeg, GedObject);

  ork::Gradient<ork::fvec4>* mGradientObject;
  GedItemNode* mParent;
  int miSeg;

public:
  void OnUiEvent(ork::ui::event_constptr_t ev) final {
    const auto& filtev = ev->mFilteredEvent;

    switch (filtev._eventcode) {
      case ui::EventCode::DOUBLECLICK: {
        // printf( "GradSplit par<%p> go<%p>\n", mParent, mGradientObject );
        if (mParent && mGradientObject) {
          orklut<float, ork::fvec4>& data = mGradientObject->Data();
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
      , mParent(0)
      , miSeg(-1) {
  }

  void SetGradientObject(ork::Gradient<ork::fvec4>* pgrad) {
    mGradientObject = pgrad;
  }
  void SetParent(GedItemNode* ppar) {
    mParent = ppar;
  }
};

void GedGradientEditSeg::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

class GedGradientV4Widget : public GedItemNode {
  static const int kh        = 48;
  static const int kpoolsize = 32;
  ork::pool<GedGradientEditPoint> mEditPoints;
  ork::pool<GedGradientEditSeg> mEditSegs;
  ork::Gradient<ork::fvec4>* mGradientObject;
  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;

  void DoDraw(lev2::Context* pTARG) // virtual
  {
    const orklut<float, ork::fvec4>& data = mGradientObject->Data();

    GetSkin()->DrawBgBox(this, miX, miY + 2, miW, kh - 3, GedSkin::ESTYLE_BACKGROUND_1);
    GedSkin::GedPrim prim;
    prim.mDrawCB = GradientCustomPrim;
    prim.mpNode  = this;
    prim.meType  = ork::lev2::EPrimitiveType::MULTI;
    prim.iy1     = miY;
    prim.iy2     = miY + kh;
    GetSkin()->AddPrim(prim);

    ////////////////////////////////////

    const int knumpoints = (int)data.size();
    const int ksegs      = knumpoints - 1;

    ////////////////////////////////////
    // draw segments

    if (pTARG->FBI()->isPickState()) {
      mEditSegs.clear();

      for (int i = 0; i < ksegs; i++) {
        std::pair<float, ork::fvec4> pointa = data.GetItemAtIndex(i);
        std::pair<float, ork::fvec4> pointb = data.GetItemAtIndex(i + 1);

        GedGradientEditSeg* editseg = mEditSegs.allocate();
        editseg->SetGradientObject(mGradientObject);
        editseg->SetParent(this);
        editseg->SetSeg(i);

        float fi0 = pointa.first;
        float fi1 = pointb.first;
        float fw  = (fi1 - fi0);

        int fx0 = miX + int(fi0 * float(miW));
        int fw0 = int(fw * float(miW));

        GetSkin()->DrawBgBox(editseg, fx0, miY, fw0, kh, GedSkin::ESTYLE_DEFAULT_CHECKBOX, 2);
        GetSkin()->DrawOutlineBox(editseg, fx0, miY, fw0, kh, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 1);
      }
    }

    ////////////////////////////////////
    // draw points

    mEditPoints.clear();

    for (int i = 0; i < knumpoints; i++) {
      std::pair<float, ork::fvec4> point = data.GetItemAtIndex(i);

      GedGradientEditPoint* editpoint = mEditPoints.allocate();
      editpoint->SetGradientObject(mGradientObject);
      editpoint->SetParent(this);
      editpoint->SetPoint(i);

      int fxc = int(point.first * float(miW));
      int fx0 = miX + (fxc - kpntsize);
      int fy0 = miY + (kh - (kpntsize * 2));

      GedSkin::ESTYLE pntstyl = IsObjectHilighted(editpoint) ? GedSkin::ESTYLE_DEFAULT_HIGHLIGHT : GedSkin::ESTYLE_DEFAULT_CHECKBOX;

      if (pTARG->FBI()->isPickState()) {
        GetSkin()->DrawBgBox(editpoint, fx0 + 1, fy0 + 1, kpntsize * 2 - 2, kpntsize * 2 - 2, pntstyl, 2);
      } else {
        GetSkin()->DrawOutlineBox(editpoint, fx0 + 1, fy0 + 1, kpntsize * 2 - 2, kpntsize * 2 - 2, pntstyl, 2);
      }
      GetSkin()->DrawOutlineBox(editpoint, fx0, fy0, kpntsize * 2, kpntsize * 2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 2);
    }

    ////////////////////////////////////
  }

  static void GradientCustomPrim(GedSkin* pskin, GedObject* pnode, ork::lev2::Context* pTARG) {
    GedGradientV4Widget* pthis            = rtti::autocast(pnode);
    const orklut<float, ork::fvec4>& data = pthis->mGradientObject->Data();
    const int knumpoints                  = (int)data.size();
    const int ksegs                       = knumpoints - 1;

    if (0 == ksegs)
      return;

    if (pTARG->FBI()->isPickState()) {
    } else if (pthis->mGradientObject) {
      lev2::GfxMaterial3DSolid material(pTARG);
      material.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
      material._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      material._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      material._rasterstate.SetBlending(ork::lev2::EBLENDING_OFF);
      material._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
      material._rasterstate.SetShadeModel(ork::lev2::ESHADEMODEL_SMOOTH);

      pthis->mVertexBuffer.Reset();
      lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
      vw.Lock(pTARG, &pthis->mVertexBuffer, 6 * ksegs);

      fvec2 uv;

      const float kz = 0.0f;

      float fx = float(pthis->miX);
      float fy = float(pthis->miY + pskin->GetScrollY());
      float fw = float(pthis->miW);
      float fh = float(pthis->kh);

      for (int i = 0; i < ksegs; i++) {
        std::pair<float, ork::fvec4> data_a = data.GetItemAtIndex(i);
        std::pair<float, ork::fvec4> data_b = data.GetItemAtIndex(i + 1);

        float fia = data_a.first;
        float fib = data_b.first;

        float fx0 = fx + (fia * fw);
        float fx1 = fx + (fib * fw);
        float fy0 = fy;
        float fy1 = fy + fh;

        const fvec4& c0 = data_a.second;
        const fvec4& c1 = data_b.second;

        lev2::SVtxV12C4T16 v0(fvec3(fx0, fy0, kz), uv, c0.GetVtxColorAsU32());
        lev2::SVtxV12C4T16 v1(fvec3(fx1, fy0, kz), uv, c1.GetVtxColorAsU32());
        lev2::SVtxV12C4T16 v2(fvec3(fx1, fy1, kz), uv, c1.GetVtxColorAsU32());
        lev2::SVtxV12C4T16 v3(fvec3(fx0, fy1, kz), uv, c0.GetVtxColorAsU32());

        vw.AddVertex(v0);
        vw.AddVertex(v1);
        vw.AddVertex(v2);

        vw.AddVertex(v0);
        vw.AddVertex(v2);
        vw.AddVertex(v3);
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
      pTARG->PushModColor(fvec3::White());
      pTARG->GBI()->DrawPrimitive(&material, vw, ork::lev2::EPrimitiveType::TRIANGLES);
      pTARG->PopModColor();
      pTARG->MTXI()->PopPMatrix();
      pTARG->MTXI()->PopVMatrix();
      pTARG->MTXI()->PopMMatrix();
      ////////////////////////////////////////////////////////////////

    } else {
      OrkAssert(false);
    }
  }

  int CalcHeight(void) {
    return kh;
  } // virtual

public:
  GedGradientV4Widget(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj)
      : GedItemNode(mdl, name, prop, obj)
      , mGradientObject(0)
      , mVertexBuffer(256, 0, ork::lev2::EPrimitiveType::TRIANGLES)
      , mEditPoints(kpoolsize)
      , mEditSegs(kpoolsize) {
    if (prop) {
      const reflect::IObjectPropertyObject* pprop = rtti::autocast(GetOrkProp());
      mGradientObject                             = rtti::autocast(pprop->Access(GetOrkObj()));
    } else {
      mGradientObject = rtti::autocast(obj);
    }
  }
  bool DoDrawDefault() const // virtual
  {
    return false;
  }
};

void GedFactoryGradient::Describe() {
}

GedItemNode*
GedFactoryGradient::CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::IObjectProperty* prop, Object* obj)
    const {
  GedItemNode* groupnode = new GedLabelNode(mdl, Name.c_str(), prop, obj);

  mdl.GetGedWidget()->PushItemNode(groupnode);

  GedItemNode* itemnode = new GedGradientV4Widget(mdl, Name.c_str(), prop, obj);

  mdl.GetGedWidget()->AddChild(itemnode);

  mdl.GetGedWidget()->PopItemNode(groupnode);

  return groupnode;
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryGradient, "ged.factory.gradient");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedGradientEditPoint, "GedGradientEditPoint");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedGradientEditSeg, "GedGradientEditSeg");
