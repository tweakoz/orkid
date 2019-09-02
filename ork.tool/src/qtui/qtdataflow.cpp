////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/dataflow/dataflow.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/basicfilters.h>
#include <ork/reflect/RegisterProperty.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/qtui/qtdataflow.h>
#include <orktool/qtui/qtmainwin.h>
#include <orktool/qtui/qtui_tool.h>
#include <pkg/ent/PerfController.h>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

using namespace ork::asset;using namespace ork::dataflow;
using namespace ork::lev2;

namespace ork::lev2 {
template <> void CPickBuffer<ork::tool::GraphVP>::Draw(lev2::GetPixelContext& ctx) {
  mPickIds.clear();

  auto tgt = GetContext();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();
  auto fxi = tgt->FXI();
  auto rsi = tgt->RSI();

  int irtgw = mpPickRtGroup->GetW();
  int irtgh = mpPickRtGroup->GetH();
  int isurfw = mpViewport->GetW();
  int isurfh = mpViewport->GetH();
  if (irtgw != isurfw || irtgh != isurfh) {
    printf("resize ged pickbuf rtgroup<%d %d>\n", isurfw, isurfh);
    this->SetBufferWidth(isurfw);
    this->SetBufferHeight(isurfh);
    tgt->SetSize(0, 0, isurfw, isurfh);
    mpPickRtGroup->Resize(isurfw, isurfh);
  }
  fbi->PushRtGroup(mpPickRtGroup);
  fbi->EnterPickState(this);
  ui::DrawEvent drwev(tgt);
  mpViewport->RePaintSurface(drwev);
  fbi->LeavePickState();
  fbi->PopRtGroup();
}
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork::tool {

DataFlowEditor* gdfloweditor = 0;

class dflowgraphedit : public tool::ged::IOpsDelegate {
  RttiDeclareConcrete(dflowgraphedit, tool::ged::IOpsDelegate);
  virtual void Execute(ork::Object* ptarget) {
    ork::dataflow::graph_inst* pgrf = rtti::autocast(ptarget);
    if (gdfloweditor && pgrf) {
      gdfloweditor->Attach(pgrf);
    }
  }
};

void dflowgraphedit::Describe() {}

///////////////////////////////////////////////////////////////////////////////

dataflow::graph_data* GraphVP::GetTopGraph() { return mDflowEditor.GetTopGraph(); }

GraphVP::GraphVP(DataFlowEditor& dfed, tool::ged::ObjModel& objmdl, const std::string& name)
    : ui::Surface(name, 0, 0, 0, 0, CColor3(0.1f, 0.1f, 0.1f), 0.0f), mObjectModel(objmdl), mDflowEditor(dfed),
      mGridMaterial(GfxEnv::GetRef().GetLoaderTarget())

{
  dflowgraphedit::GetClassStatic();

  gdfloweditor = &dfed;

  mGrid.SetCenter(fvec2(0.0f, 0.0f));
  mGrid.SetExtent(100.0f);
  mGrid.SetZoom(1.0f);

  mpArrowTex = AssetManager<TextureAsset>::Load("lev2://textures/dfarrow")->GetTexture();

  mObjectModel.EnablePaint();

  auto ptimer = new Timer;

  auto lamb_outer = [=]() {
    auto lamb_inner = [=]() { this->SetDirty(); };
    Op(lamb_inner).QueueASync(MainThreadOpQ());
  };

  ptimer->OnInterval(0.3f, lamb_outer);

  // object::Connect( & mObjectModel.GetSigRepaint(), & mWidget.GetSlotRepaint() );
  // object::Connect( & mObjectModel.GetSigModelInvalidated(), & mDflowEditor.GetSlotModelInvalidated() );

  /*object::ConnectToLambda( & mObjectModel.GetSigModelInvalidated(),
      LambdaSlot,
      [=](Object* pobj)
      {
          assert(false);
      });*/
}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::draw_connections(GfxTarget* pTARG) {
  auto fbi = pTARG->FBI();
  bool is_pick = fbi->IsPickState();

  if (nullptr == GetTopGraph())
    return;
  const auto& modules = GetTopGraph()->Modules();
  auto& VB = lev2::GfxEnv::GetSharedDynamicVB();
  fvec4 uv0(0.0f, 0.0f,0,0);
  fvec4 uv1(1.0f, 0.0f,0,0);
  fvec4 uv2(1.0f, 1.0f,0,0);
  fvec4 uv3(0.0f, 1.0f,0,0);
  float fw(kvppickdimw);
  float fh(kvppickdimw);
  float fwd2 = fw * 0.5f;
  float fhd2 = fh * 0.5f;
  float faspect = float(miW) / float(miH);
  if (false == is_pick) {
    /////////////////////////////////
    // wires
    /////////////////////////////////
    int ivcount = 0;
    // count the number of verts we will use
    for (const auto& item : modules) {
      dataflow::dgmodule* pmod = rtti::autocast(item.second);
      if (pmod) {
        int inuminps = pmod->GetNumInputs();
        for (int ip = 0; ip < inuminps; ip++) {
          dataflow::inplugbase* pinp = pmod->GetInput(ip);
          const dataflow::outplugbase* poutplug = pinp->GetExternalOutput();
          if (poutplug)
            ivcount += 6;
        }
      }
    }
    if (ivcount) {
      lev2::VtxWriter<lev2::SVtxV16T16C16> vw;
      vw.Lock(pTARG, &VB, ivcount);
      for (auto it : modules) {
        dataflow::dgmodule* pmod = rtti::autocast(it.second);
        if (pmod) {
          const fvec2& pos = pmod->GetGVPos();
          int inuminps = pmod->GetNumInputs();
          for (int ip = 0; ip < inuminps; ip++) {
            dataflow::inplugbase* pinp = pmod->GetInput(ip);
            const dataflow::outplugbase* poutplug = pinp->GetExternalOutput();
            if (poutplug) {
              fvec4 ucolor = fvec4(1,1,1,1);
              fvec4 ucolor2 = fvec4(1,1,1,1);
              dataflow::dgmodule* pothmod = rtti::autocast(poutplug->GetModule());
              const fvec2& othpos = pothmod->GetGVPos();
              fvec3 vdif = (othpos - pos);
              fvec3 vdir = vdif.Normal();
              fvec3 vcross = vdir.Cross(fvec3(0.0f, 0.0f, 1.0f)) * fvec3(1.0f, faspect, 0.0f) * 5.0f;
              float flength = vdif.Mag();
              fvec4 uvs(flength / 16.0f, 1.0f,0,0);
              SVtxV16T16C16 v0(fvec3(pos + vcross.GetXY()), uv1 * uvs, ucolor2);
              SVtxV16T16C16 v1(fvec3(othpos + vcross.GetXY()), uv0 * uvs, ucolor);
              SVtxV16T16C16 v2(fvec3(othpos - vcross.GetXY()), uv3 * uvs, ucolor);
              SVtxV16T16C16 v3(fvec3(pos - vcross.GetXY()), uv2 * uvs, ucolor2);
              vw.AddVertex(v0);
              vw.AddVertex(v1);
              vw.AddVertex(v2);
              vw.AddVertex(v0);
              vw.AddVertex(v2);
              vw.AddVertex(v3);
            }
          }
        }
      }
      vw.UnLock(pTARG);
      mGridMaterial.SetTexture(mpArrowTex);
      mGridMaterial.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);
      mGridMaterial.mRasterState.SetDepthTest(lev2::EDEPTHTEST_OFF);
      mGridMaterial.mRasterState.SetAlphaTest(EALPHATEST_OFF, 0.0f);
      mGridMaterial.mRasterState.SetBlending(lev2::EBLENDING_OFF);
      pTARG->BindMaterial(&mGridMaterial);
      pTARG->GBI()->DrawPrimitive(vw, EPRIM_TRIANGLES, ivcount);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

struct regstr {
  fvec3 pos;
  int ser;
  int ireg;
};

void GraphVP::DoInit(lev2::GfxTarget* pt) {
  auto fbi = pt->FBI();
  auto par = fbi->GetThisBuffer();
  mpPickBuffer = new lev2::CPickBuffer<GraphVP>(par, this, 0, 0, miW, miH, lev2::PickBufferBase::EPICK_FACE_VTX);

  mpPickBuffer->CreateContext();
  mpPickBuffer->RefClearColor().SetRGBAU32(0);
  mpPickBuffer->GetContext()->FBI()->SetClearColor(CColor4(0.0f, 0.0f, 0.0f, 0.0f));
}
void GraphVP::DoRePaintSurface(ui::DrawEvent& drwev) {
  auto tgt = drwev.GetTarget();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();
  auto fxi = tgt->FXI();
  auto rsi = tgt->RSI();
  auto gbi = tgt->GBI();
  auto& primi = lev2::CGfxPrimitives::GetRef();
  auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();
  auto& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  bool has_foc = HasMouseFocus();
  bool is_pick = fbi->IsPickState();
  auto& modules = GetTopGraph()->Modules();

  if (nullptr == GetTopGraph()) {
    fbi->PushScissor(SRect(0, 0, miW, miH));
    fbi->PushViewport(SRect(0, 0, miW, miH));
    fbi->Clear(fvec4::Black(), 1.0f);
    fbi->PopViewport();
    fbi->PopScissor();
    return;
  }

  // SRect tgt_rect = SRect( 0,0, pTARG->GetW(), pTARG->GetH() );
  // RenderContextFrameData framedata;
  // framedata.SetDstRect(tgt_rect);
  // framedata.SetTarget( pTARG );

  // pTARG->SetRenderContextFrameData( & framedata );

  fvec4 uv0(0.0f, 0.0f,0,0);
  fvec4 uv1(1.0f, 0.0f,0,0);
  fvec4 uv2(1.0f, 1.0f,0,0);
  fvec4 uv3(0.0f, 1.0f,0,0);

  //////////////////////////////////////////

  float fmodsizew = 25.0f;
  float fmodsizeh = 25.0f;

  fvec2 of0(-fmodsizew, -fmodsizeh);
  fvec2 of1(+fmodsizew, -fmodsizeh);
  fvec2 of2(+fmodsizew, +fmodsizeh);
  fvec2 of3(-fmodsizew, +fmodsizeh);

  float fw(kvppickdimw);
  float fh(kvppickdimw);
  float fwd2 = fw * 0.5f;
  float fhd2 = fh * 0.5f;

  mGridMaterial.mRasterState.SetDepthTest(lev2::EDEPTHTEST_OFF);
  mGridMaterial.mRasterState.SetAlphaTest(EALPHATEST_GREATER, 0.0f);
  //	mGridMaterial.mRasterState.SetAlphaTest( EALPHATEST_OFF, 0.0f );

  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  fbi->PushScissor(SRect(0, 0, miW, miH));
  fbi->PushViewport(SRect(0, 0, miW, miH));
  {

    vector<regstr> regstrs;

    fbi->Clear(fvec4::Blue(), 1.0f);
    mtxi->PushPMatrix(mGrid.GetOrthoMatrix());
    mtxi->PushVMatrix(CMatrix4::Identity);
    mtxi->PushMMatrix(CMatrix4::Identity);
    {
      uint64_t pickID = mpPickBuffer->AssignPickId(GetTopGraph());
      fvec4 color(1, 1, 1, 1);
      if (is_pick)
        color.SetRGBAU64(pickID);

      ///////////////////////////////////////////////////////
      // draw background
      ///////////////////////////////////////////////////////

      float fxa = mGrid.GetTopLeft().GetX();
      float fxb = mGrid.GetBotRight().GetX();
      float fya = mGrid.GetTopLeft().GetY();
      float fyb = mGrid.GetBotRight().GetY();

      SVtxV16T16C16 v0(fvec3(fxa, fya, 0.0f), uv0, color);
      SVtxV16T16C16 v1(fvec3(fxb, fya, 0.0f), uv1, color);
      SVtxV16T16C16 v2(fvec3(fxb, fyb, 0.0f), uv2, color);
      SVtxV16T16C16 v3(fvec3(fxa, fyb, 0.0f), uv3, color);

      // int ivbbase = vbuf.GetNum();
      {

        lev2::VtxWriter<lev2::SVtxV16T16C16> vw;
        vw.Lock(tgt, &VB, 6);
        {
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);

          vw.AddVertex(v0);
          vw.AddVertex(v2);
          vw.AddVertex(v3);
        }
        vw.UnLock(tgt);

        static const char* assetname = "lev2://textures/dfnodebg2";
        static lev2::TextureAsset* ptexasset = asset::AssetManager<lev2::TextureAsset>::Load(assetname);

        mGridMaterial.SetTexture(ptexasset->GetTexture());

        mGridMaterial.SetColorMode(is_pick ? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR
                                           : lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);

        mGridMaterial.mRasterState.SetBlending(lev2::EBLENDING_OFF);

        tgt->BindMaterial(&mGridMaterial);

        gbi->DrawPrimitive(vw, EPRIM_TRIANGLES, 6);
      }

      ///////////////////////////////////////////////////////
      // draw grid
      ///////////////////////////////////////////////////////

      if (false == is_pick)
        mGrid.Render(tgt, miW, miH);

      ///////////////////////////////////////////////////////

      draw_connections(tgt);

      ///////////////////////////////////////////////////////
      // draw modules
      ///////////////////////////////////////////////////////

      tgt->PushMaterial(&mGridMaterial);

      int inummod = (int)modules.size();

      int imod = 0;
      for (const auto& item : modules) {
        dataflow::dgmodule* pmod = rtti::autocast(item.second);

        if (pmod) {
          auto module_class = pmod->GetClass();
          auto class_desc = module_class->Description();

          const fvec2& pos = pmod->GetGVPos();

          uint64_t pickID = mpPickBuffer->AssignPickId(pmod);

          color = fvec4(1,1,1,1);
          if( is_pick )
            color.SetRGBAU64(pickID);

          SVtxV16T16C16 v0(fvec3(pos + of0), uv0, color);
          SVtxV16T16C16 v1(fvec3(pos + of1), uv1, color);
          SVtxV16T16C16 v2(fvec3(pos + of2), uv2, color);
          SVtxV16T16C16 v3(fvec3(pos + of3), uv3, color);

          lev2::VtxWriter<lev2::SVtxV16T16C16> vw;
          vw.Lock(tgt, &VB, 6);

          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);

          vw.AddVertex(v0);
          vw.AddVertex(v2);
          vw.AddVertex(v3);

          vw.UnLock(tgt);

          //////////////////////
          // select texture (using dynamic interface if requested)
          //////////////////////

          lev2::Texture* picon = 0;

          bool do_blend = false;

          auto shbanno = class_desc.GetClassAnnotation("dflowshouldblend");
          if (shbanno.IsA<bool>()) {
            do_blend = shbanno.Get<bool>();
          }

          if (false == is_pick) {
            auto iconcbanno = class_desc.GetClassAnnotation("dflowicon");

            typedef lev2::Texture* (*icon_cb_t)(dataflow::dgmodule*);

            if (iconcbanno.IsA<icon_cb_t>()) {
              auto IconCB = iconcbanno.Get<icon_cb_t>();

              picon = IconCB(pmod);
            }
          }

          mGridMaterial.SetColorMode(is_pick ? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR
                                             : (picon != 0) ? lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR
                                                            : lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);

          do_blend &= (false == is_pick);

          auto blend_mode = do_blend ? lev2::EBLENDING_ALPHA : lev2::EBLENDING_OFF;
          mGridMaterial.mRasterState.SetBlending(blend_mode);
          mGridMaterial.SetTexture(picon);

          //////////////////////
          // draw the dataflow node
          //////////////////////

          fxi->InvalidateStateBlock();

          gbi->DrawPrimitive(vw, EPRIM_TRIANGLES);

          //////////////////////
          // enqueue register mapping for draw
          //////////////////////

          int ireg = pmod->GetOutput(0)->GetRegister() ? pmod->GetOutput(0)->GetRegister()->mIndex : -1;

          regstr rs;
          rs.pos = pos;
          rs.ser = pmod->Key().mSerial;
          rs.ireg = ireg;
          regstrs.push_back(rs);

        }
        imod++;
      }
      tgt->PopMaterial();
    }
    mtxi->PopPMatrix();
    mtxi->PopVMatrix();
    mtxi->PopMMatrix();

    ////////////////////////////////////////////////////////////////

    mtxi->PushUIMatrix(miW, miH);
    if (false == is_pick) {
      lev2::CFontMan::BeginTextBlock(tgt);
      tgt->PushModColor(CColor4::Yellow());
      {
        lev2::CFontMan::DrawText(tgt, 8, 8, "GroupDepth<%d>", mDflowEditor.StackDepth());
        if (mDflowEditor.GetSelModule()) {
          dataflow::dgmodule* pdgmod = mDflowEditor.GetSelModule();
          lev2::CFontMan::DrawText(tgt, 8, 16, "Sel<%s>", pdgmod->GetName().c_str());
        }

        float fxa = mGrid.GetTopLeft().GetX();
        float fxb = mGrid.GetBotRight().GetX();
        float fya = mGrid.GetTopLeft().GetY();
        float fyb = mGrid.GetBotRight().GetY();
        float fgw = fxb - fxa;
        float fgh = fyb - fya;
        float ftw = miW;
        float fth = miH;
        float fwr = ftw / fgw;
        float fhr = fth / fgh;
        float fzoom = mGrid.GetZoom();

        float fcx = mGrid.GetCenter().GetX();
        float fcy = mGrid.GetCenter().GetY();

        int inumrs = regstrs.size();
        for (int i = 0; i < inumrs; i++) {
          const regstr& rs = regstrs[i];

          const fvec2 pos = rs.pos; //(pos-xy0);

          float fxx = (pos.GetX() - fxa) * (ftw / fgw);
          float fyy = (pos.GetY() - fya) * (fth / fgh);

          int imx = int(fxx);
          int imy = int(fyy);

          float ioff = fmodsizew * (ftw / fgw);

          if (false == is_pick) {
            lev2::CFontMan::DrawText(tgt, imx + ioff, imy + ioff, "%d:%d", rs.ser, rs.ireg);
          }
        }
      }
      lev2::CFontMan::EndTextBlock(tgt);
      tgt->PopModColor();
    }
    mtxi->PopUIMatrix();

    ////////////////////////////////////////////////////////////////
  }
  fbi->PopViewport();
  fbi->PopScissor();
  ////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::ReCenter() {
  dataflow::graph_data* pgrf = mDflowEditor.GetTopGraph();
  if (pgrf) {
    fvec2 vmin(+CFloat::TypeMax(), +CFloat::TypeMax());
    fvec2 vmax(-CFloat::TypeMax(), -CFloat::TypeMax());
    const orklut<ork::PoolString, ork::Object*>& modules = pgrf->Modules();
    for (orklut<ork::PoolString, ork::Object*>::const_iterator it = modules.begin(); it != modules.end(); it++) {
      dataflow::dgmodule* pmod = rtti::autocast(it->second);
      if (pmod) {
        const fvec2& pos = pmod->GetGVPos();

        if (pos.GetX() < vmin.GetX())
          vmin.SetX(pos.GetX());
        if (pos.GetY() < vmin.GetY())
          vmin.SetY(pos.GetY());
        if (pos.GetX() > vmax.GetX())
          vmax.SetX(pos.GetX());
        if (pos.GetY() > vmax.GetY())
          vmax.SetY(pos.GetY());
      }
    }

    fvec2 rng = vmax - vmin;
    fvec2 ctr = (vmin + vmax) * 0.5f;
    float fmax = (rng.GetX() > rng.GetY()) ? rng.GetX() : rng.GetY();
    float fz = (mGrid.GetExtent()) / (fmax + 32.0f);
    mGrid.SetCenter(ctr);
    mGrid.SetZoom(fz);
  }
  SetDirty();
}

///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult GraphVP::DoOnUiEvent(const ui::Event& EV) {
  const auto& filtev = EV.mFilteredEvent;

  int ix = EV.miX;
  int iy = EV.miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);
  float fx = float(ilocx) / float(GetW());
  float fy = float(ilocy) / float(GetH());

  lev2::GetPixelContext ctx;
  ctx.miMrtMask = (1 << 0) | (1 << 1); // ObjectID and ObjectUVD
  ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR64;
  ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

  QInputEvent* qip = (QInputEvent*)EV.mpBlindEventData;

  bool bisshift = EV.mbSHIFT;
  bool bisalt = EV.mbALT;
  bool bisctrl = EV.mbCTRL;

  static dataflow::dgmodule* gpmodule = 0;

  static fvec2 gbasexym;
  static fvec2 gbasexy;

  switch (filtev.miEventCode) {
    case ui::UIEV_KEY: {
      if (filtev.miKeyCode == 'a') {
        ReCenter();
      }
    }
    case ui::UIEV_MOVE: {
      SetDirty();
      break;
    }
    case ui::UIEV_DRAG: {
      if (bisalt || filtev.mBut1) {
        mGrid.SetCenter(gbasexy - (fvec2(ix, iy) - gbasexym));
      } else if (gpmodule) {
        float fix = (fx * mGrid.GetBotRight().GetX()) + ((1.0 - fx) * mGrid.GetTopLeft().GetX());
        float fiy = (fy * mGrid.GetBotRight().GetY()) + ((1.0 - fy) * mGrid.GetTopLeft().GetY());
        gpmodule->SetGVPos(mGrid.Snap(fvec2(fix, fiy)));
      }
      SetDirty();
      break;
    }
    case ui::UIEV_RELEASE: {
      gpmodule = 0;
      break;
    }
    case ui::UIEV_PUSH: {
      if (false == bisctrl && filtev.mBut0) {
        GetPixel(ilocx, ilocy, ctx);
        ork::Object* pobj = (ork::Object*)ctx.GetObject(mpPickBuffer, 0);

        printf("dflow pick pobj<%p>\n", pobj);
        if (ork::Object* object = ork::rtti::autocast(pobj))
          mObjectModel.Attach(object);
        gpmodule = rtti::autocast(pobj);
        gbasexym = fvec2(ix, iy);
        gbasexy = mGrid.GetCenter();
        dataflow::dgmodule* dgmod = rtti::autocast(pobj);
        mDflowEditor.SelModule(dgmod);
      }
      SetDirty();
      break;
    }
    case ui::UIEV_DOUBLECLICK: {
      GetPixel(ilocx, ilocy, ctx);
      ork::rtti::ICastable* pobj = ctx.GetObject(mpPickBuffer, 0);
      gpmodule = rtti::autocast(pobj);

      if (bisctrl) {
        if (gpmodule && gpmodule->IsGroup()) {
          mDflowEditor.Push(gpmodule->GetChildGraph());
          ReCenter();
        } else {
          mDflowEditor.Pop();
          ReCenter();
        }
      } else {
        mDflowEditor.SetProbeModule(gpmodule);
      }
      SetDirty();
      break;
    }
    case ui::UIEV_MOUSEWHEEL: {
      QWheelEvent* qem = (QWheelEvent*)qip;

      int idelta = qem->delta();

#if defined(_DARWIN)
      const float kstep = bisshift ? (90.0f / 100.0f) : (97.0f / 100.0f); // trackpad
#else
      const float kstep = 95.0f / 100.0f;
#endif

      float fdelta = 1.0f;
      if (idelta > 0)
        fdelta = 1.0f / kstep;
      else if (idelta < 0)
        fdelta = kstep;

      float fz = mGrid.GetZoom() * fdelta;
      if (fz < 0.03f)
        fz = 0.03f;
      if (fz > 10.0f)
        fz = 10.0f;
      mGrid.SetZoom(fz);
      SetDirty();
    }
  }
  return ui::HandlerResult(this);
}

///////////////////////////////////////////////////////////////////////////////

DataFlowEditor::DataFlowEditor() : mGraphVP(0), mpSelModule(0), mpProbeModule(0), ConstructAutoSlot(ModelInvalidated) {}

void DataFlowEditor::Describe() {
  reflect::RegisterFunctor("SlotClear", &DataFlowEditor::SlotClear);
  RegisterAutoSlot(DataFlowEditor, ModelInvalidated);
}

void DataFlowEditor::Attach(dataflow::graph_data* pgrf) {
  while (mGraphStack.empty() == false)
    mGraphStack.pop();
  mGraphStack.push(pgrf);
}
void DataFlowEditor::Push(dataflow::graph_data* pgrf) { mGraphStack.push(pgrf); }
void DataFlowEditor::Pop() {
  if (mGraphStack.size() > 1) {
    mGraphStack.pop();
  }
}
void DataFlowEditor::SlotClear() {
  while (mGraphStack.empty() == false)
    mGraphStack.pop();
}
void DataFlowEditor::SlotModelInvalidated() {
  while (mGraphStack.empty() == false)
    mGraphStack.pop();
}
dataflow::graph_data* DataFlowEditor::GetTopGraph() { return mGraphStack.empty() ? 0 : mGraphStack.top(); }

} // namespace ork::tool

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::dflowgraphedit, "dflowgraphedit");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DataFlowEditor, "DataFlowEditor");
