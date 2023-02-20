///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/reflect/enum_serializer.inl>
#include <ork/math/polar.h>
#include <ork/math/plane.hpp>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::proctex::Cells, "proctex::Cells");
ImplementReflectionX(ork::proctex::Octaves, "proctex::Octaves");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace proctex {
///////////////////////////////////////////////////////////////////////////////
void Octaves::describeX(class_t* clazz) {
  /*
  RegisterObjInpPlug(Octaves, Input);
  RegisterFloatXfPlug(Octaves, BaseOffsetX, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, BaseOffsetY, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, ScalOffsetX, -4.0f, 4.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, ScalOffsetY, -4.0f, 4.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, BaseFreq, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, BaseAmp, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, ScalFreq, -4.0f, 4.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Octaves, ScalAmp, -4.0f, 4.0f, ged::OutPlugChoiceDelegate);

  ork::reflect::RegisterProperty("NumOctaves", &Octaves::miNumOctaves);
  ork::reflect::annotatePropertyForEditor<Octaves>("NumOctaves", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<Octaves>("NumOctaves", "editor.range.max", "10");

  static const char* EdGrpStr = "grp://Basic Input NumOctaves "
                                "grp://Plugs BaseOffsetX ScalOffsetX BaseOffsetY ScalOffsetY BaseFreq ScalFreq BaseAmp ScalAmp ";
  reflect::annotateClassForEditor<Octaves>("editor.prop.groups", EdGrpStr);
  */
}
///////////////////////////////////////////////////////////////////////////////
Octaves::Octaves()
    : ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(BaseOffsetX, dataflow::EPR_UNIFORM, mfBaseOffsetX)
    , ConstructInpPlug(BaseOffsetY, dataflow::EPR_UNIFORM, mfBaseOffsetY)
    , ConstructInpPlug(ScalOffsetX, dataflow::EPR_UNIFORM, mfScalOffsetX)
    , ConstructInpPlug(ScalOffsetY, dataflow::EPR_UNIFORM, mfScalOffsetY)
    , ConstructInpPlug(BaseFreq, dataflow::EPR_UNIFORM, mfBaseFreq)
    , ConstructInpPlug(ScalFreq, dataflow::EPR_UNIFORM, mfScalFreq)
    , ConstructInpPlug(BaseAmp, dataflow::EPR_UNIFORM, mfBaseAmp)
    , ConstructInpPlug(ScalAmp, dataflow::EPR_UNIFORM, mfScalAmp)
    , mOctMaterial(lev2::contextForCurrentThread(), "orkshader://proctex", "octaves") {

  mfBaseFreq    = (1.0f);
  mfScalFreq    = (2.0f);
  mfBaseAmp     = (1.0f);
  mfScalAmp     = (0.5f);
  mfScalOffsetX = (1.0f);
  mfScalOffsetY = (1.0f);
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* Octaves::GetInput(int idx) const {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpBaseOffsetX;
      break;
    case 2:
      rval = &mPlugInpBaseOffsetY;
      break;
    case 3:
      rval = &mPlugInpScalOffsetX;
      break;
    case 4:
      rval = &mPlugInpScalOffsetY;
      break;
    case 5:
      rval = &mPlugInpBaseFreq;
      break;
    case 6:
      rval = &mPlugInpBaseAmp;
      break;
    case 7:
      rval = &mPlugInpScalFreq;
      break;
    case 8:
      rval = &mPlugInpScalAmp;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void Octaves::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::Octaves::compute"));
  Buffer& buffer = GetWriteBuffer(ptex);
  // printf( "Octaves wrbuf<%p> wrtex<%p>\n", & buffer, buffer.OutputTexture() );
  const ImgOutPlug* conplug = rtti::autocast(mPlugInpInput.GetExternalOutput());
  if (conplug) {
    mOctMaterial.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
    mOctMaterial._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    mOctMaterial._rasterstate.SetCullTest(ork::lev2::ECullTest::OFF);
    mOctMaterial._rasterstate.SetBlending(ork::lev2::Blending::ADDITIVE);
    mOctMaterial._rasterstate.SetDepthTest(ork::lev2::EDepthTest::ALWAYS);
    mOctMaterial._rasterstate.SetZWriteMask(false);

    auto inptex = conplug->GetValue().GetTexture(ptex);

    inptex->TexSamplingMode().PresetTrilinearWrap();
    pTARG->TXI()->ApplySamplingMode(inptex);

    mOctMaterial.SetTexture(inptex);

    float sina = 0.0f;
    float cosa = 1.0f;

    mOctMaterial.SetUser0(fvec4(sina, cosa, 0.0f, float(buffer.miW)));
    ////////////////////////////////////
    float ffrq = mPlugInpBaseFreq.GetValue();
    float famp = mPlugInpBaseAmp.GetValue();
    float offx = mPlugInpBaseOffsetX.GetValue();
    float offy = mPlugInpBaseOffsetY.GetValue();
    buffer.PtexBegin(pTARG, true, true);
    for (int i = 0; i < miNumOctaves; i++) {
      fmtx4 mtxR, mtxS, mtxT;

      mtxT.setTranslation(offx, offy, 0.0f);
      mtxS.scale(ffrq, ffrq, famp);
      mtxR.setRotateZ(0.0f);

      mOctMaterial.SetAuxMatrix(fmtx4::multiply_ltor(mtxS,mtxT));
      {
        // printf( "DrawUnitTexQuad oct<%d>\n", i );
        UnitTexQuad(&mOctMaterial, pTARG);
      }
      ffrq *= mPlugInpScalFreq.GetValue();
      famp *= mPlugInpScalAmp.GetValue();
      offx *= mPlugInpScalOffsetX.GetValue();
      offy *= mPlugInpScalOffsetY.GetValue();
    }
    buffer.PtexEnd(true);
    ////////////////////////////////////
  }
  MarkClean();
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
void Cells::describeX(class_t* clazz) {
  /*ork::reflect::RegisterProperty("SeedA", &Cells::miSeedA);
  ork::reflect::annotatePropertyForEditor<Cells>("SeedA", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<Cells>("SeedA", "editor.range.max", "1000");

  ork::reflect::RegisterProperty("SeedB", &Cells::miSeedB);
  ork::reflect::annotatePropertyForEditor<Cells>("SeedB", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<Cells>("SeedB", "editor.range.max", "1000");

  ork::reflect::RegisterProperty("DimU", &Cells::miDimU);
  ork::reflect::annotatePropertyForEditor<Cells>("DimU", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<Cells>("DimU", "editor.range.max", "7");

  ork::reflect::RegisterProperty("DimV", &Cells::miDimV);
  ork::reflect::annotatePropertyForEditor<Cells>("DimV", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<Cells>("DimV", "editor.range.max", "7");

  ork::reflect::RegisterProperty("Divs", &Cells::miDiv);
  ork::reflect::annotatePropertyForEditor<Cells>("Divs", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<Cells>("Divs", "editor.range.max", "16");

  ork::reflect::RegisterProperty("Smoothing", &Cells::miSmoothing);
  ork::reflect::annotatePropertyForEditor<Cells>("Smoothing", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<Cells>("Smoothing", "editor.range.max", "8");

  RegisterFloatXfPlug(Cells, Dispersion, 0.001f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Cells, SeedLerp, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Cells, SmoothingRadius, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);

  ork::reflect::RegisterProperty("AntiAlias", &Cells::mbAA);

  static const char* EdGrpStr = "grp://Basic AntiAlias SeedA SeedB DimU DimV Divs Smoothing "
                                "grp://Plugs Dispersion SeedLerp SmoothingRadius ";

  reflect::annotateClassForEditor<Cells>("editor.prop.groups", EdGrpStr);
  */
}
///////////////////////////////////////////////////////////////////////////////
Cells::Cells()
    : mPlugInpDispersion(this, dataflow::EPR_UNIFORM, mfDispersion, "di")
    , mPlugInpSeedLerp(this, dataflow::EPR_UNIFORM, mfSeedLerp, "sl")
    , mPlugInpSmoothingRadius(this, dataflow::EPR_UNIFORM, mfSmoothingRadius, "sr")
    , mVertexBuffer(65536, 0, ork::lev2::PrimitiveType::MULTI) {
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* Cells::GetInput(int idx) const {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpDispersion;
      break;
    case 1:
      rval = &mPlugInpSeedLerp;
      break;
    case 2:
      rval = &mPlugInpSmoothingRadius;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void Cells::ComputeVB(lev2::Context* pTARG) {
  fvec3 wrapu(float(miDimU), 0.0f, 0.0f);
  fvec3 wrapv(0.0f, float(miDimV), 0.0f);
  ////////////////////////////////////////////////
  int ivecsize = miDimU * miDimV;
  mSitesA.resize(ivecsize);
  mSitesB.resize(ivecsize);
  mPolys.resize(ivecsize);
  for (int ix = 0; ix < miDimU; ix++) {
    for (int iy = 0; iy < miDimV; iy++) {
      mPolys[site_index(ix, iy)].SetDefault();
      mPolys[site_index(ix, iy)].AddVertex(fvec3(-100.0f, -100.0f, 0.0f));
      mPolys[site_index(ix, iy)].AddVertex(fvec3(+100.0f, -100.0f, 0.0f));
      mPolys[site_index(ix, iy)].AddVertex(fvec3(+100.0f, +100.0f, 0.0f));
      mPolys[site_index(ix, iy)].AddVertex(fvec3(-100.0f, +100.0f, 0.0f));
    }
  }
  srand(miSeedA);
  for (int ix = 0; ix < miDimU; ix++) {
    for (int iy = 0; iy < miDimV; iy++) {
      float ifx                   = float(rand() % miDiv) / float(miDiv);
      float fx                    = float(ix) + ifx * mPlugInpDispersion.GetValue();
      float ify                   = float(rand() % miDiv) / float(miDiv);
      float fy                    = float(iy) + ify * mPlugInpDispersion.GetValue();
      mSitesA[site_index(ix, iy)] = fvec3(fx, fy, 0.0f);
    }
  }
  srand(miSeedB);
  for (int ix = 0; ix < miDimU; ix++) {
    for (int iy = 0; iy < miDimV; iy++) {
      float ifx                   = float(rand() % miDiv) / float(miDiv);
      float fx                    = float(ix) + ifx * mPlugInpDispersion.GetValue();
      float ify                   = float(rand() % miDiv) / float(miDiv);
      float fy                    = float(iy) + ify * mPlugInpDispersion.GetValue();
      mSitesB[site_index(ix, iy)] = fvec3(fx, fy, 0.0f);
    }
  }
  float flerp = mPlugInpSeedLerp.GetValue();
  ////////////////////////////////////////////////
  mVertexBuffer.Reset();
  mVW.Lock(pTARG, &mVertexBuffer, 3 * 8 * (1 + miSmoothing) * miDimU * miDimV);
  ////////////////////////////////////////////////
  for (int ix = 0; ix < miDimU; ix++) {
    for (int iy = 0; iy < miDimV; iy++) {
      const fvec3& Site0A     = mSitesA[site_index(ix, iy)];
      const fvec3& Site0B     = mSitesB[site_index(ix, iy)];
      static const int kmaxpc = 64;
      CellPoly polychain[kmaxpc];
      int polychi        = 0;
      polychain[polychi] = mPolys[site_index(ix, iy)];
      for (int iox = -2; iox < 3; iox++) {
        int ixb     = ix + iox;
        bool bwrapL = (ixb < 0);
        bool bwrapR = (ixb >= miDimU);
        ixb         = bwrapL ? ixb + miDimU : ixb % miDimU;
        for (int ioy = -2; ioy < 3; ioy++) {
          if (iox != 0 || ioy != 0) {
            int iyb      = iy + ioy;
            bool bwrapT  = (iyb < 0);
            bool bwrapB  = (iyb >= miDimV);
            iyb          = bwrapT ? iyb + miDimV : iyb % miDimV;
            fvec3 Site1A = mSitesA[site_index(ixb, iyb)];
            fvec3 Site1B = mSitesB[site_index(ixb, iyb)];
            if (bwrapL) {
              Site1A -= wrapu;
              Site1B -= wrapu;
            }
            if (bwrapR) {
              Site1A += wrapu;
              Site1B += wrapu;
            }
            if (bwrapB) {
              Site1A += wrapv;
              Site1B += wrapv;
            }
            if (bwrapT) {
              Site1A -= wrapv;
              Site1B -= wrapv;
            }

            fvec3 CenterA = (Site0A + Site1A) * 0.5f;
            fvec3 DirA    = (Site0A - Site1A).normalized();

            fvec3 CenterB = (Site0B + Site1B) * 0.5f;
            fvec3 DirB    = (Site0B - Site1B).normalized();

            fvec3 Center;
            Center.lerp(CenterA, CenterB, flerp);
            fvec3 Dir;
            Dir.lerp(DirA, DirB, flerp);
            Dir.normalizeInPlace();

            fplane3 plane(Dir, Center);
            plane.ClipPoly(polychain[polychi], polychain[polychi + 1]);
            polychi++;
          }
        }
      }
      OrkAssert(polychi < kmaxpc);
      const CellPoly& outpoly = polychain[polychi];
      fvec3 vctr;
      vctr.lerp(Site0A, Site0B, flerp);

      ////////////////////////////////////////////////
      if (miSmoothing) {
        float smoothrad = mPlugInpSmoothingRadius.GetValue();

        for (int iv = 0; iv < outpoly.GetNumVertices(); iv++) {
          int ivp = (iv - 1);
          if (ivp < 0)
            ivp += outpoly.GetNumVertices();
          int ivn = (iv + 1) % outpoly.GetNumVertices();
          fvec3 p0;
          p0.lerp(vctr, outpoly.GetVertex(iv).Pos(), smoothrad);
          fvec3 pp;
          pp.lerp(vctr, outpoly.GetVertex(ivp).Pos(), smoothrad);
          fvec3 pn;
          pn.lerp(vctr, outpoly.GetVertex(ivn).Pos(), smoothrad);

          for (int is = 0; is < miSmoothing; is++) {
            float fu0 = (float(is) / float(miSmoothing));
            float fu1 = (float(is + 1) / float(miSmoothing));

            fvec3 p_0 = (pp + p0) * 0.5f;
            fvec3 n_0 = (p0 + pn) * 0.5f;

            fvec3 p_p_0;
            p_p_0.lerp(p_0, p0, fu0);
            fvec3 p_p_1;
            p_p_1.lerp(p_0, p0, fu1);

            fvec3 p_n_0;
            p_n_0.lerp(p0, n_0, fu0);
            fvec3 p_n_1;
            p_n_1.lerp(p0, n_0, fu1);

            fvec3 p_s_0;
            p_s_0.lerp(p_p_0, p_n_0, fu0);
            fvec3 p_s_1;
            p_s_1.lerp(p_p_1, p_n_1, fu1);

            mVW.AddVertex(ork::lev2::SVtxV12C4T16(vctr, fvec2(), 0xffffffff));
            mVW.AddVertex(ork::lev2::SVtxV12C4T16(p_s_0, fvec2(), 0));
            mVW.AddVertex(ork::lev2::SVtxV12C4T16(p_s_1, fvec2(), 0));
          }
        }
      } else
        for (int iv = 0; iv < outpoly.GetNumVertices(); iv++) {
          float fi = float(iv) / float(outpoly.GetNumVertices() - 1);
          fvec4 clr(fi, fi, fi, fi);
          int ivi  = (iv + 1) % outpoly.GetNumVertices();
          fvec3 p0 = outpoly.GetVertex(iv).Pos();
          fvec3 p1 = outpoly.GetVertex(ivi).Pos();
          mVW.AddVertex(ork::lev2::SVtxV12C4T16(vctr, fvec2(), 0xffffffff));
          mVW.AddVertex(ork::lev2::SVtxV12C4T16(p0, fvec2(), 0));
          mVW.AddVertex(ork::lev2::SVtxV12C4T16(p1, fvec2(), 0));
        }
    }
  }
  mVW.UnLock(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void Cells::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::Cells::compute"));
  Buffer& buffer = GetWriteBuffer(ptex);
  ////////////////////////////////////////////////////////////////
  dataflow::node_hash testhash;
  testhash.Hash(miDimU);
  testhash.Hash(miDimV);
  testhash.Hash(miDiv);
  testhash.Hash(miSeedA);
  testhash.Hash(miSeedB);
  testhash.Hash(miSmoothing);
  testhash.Hash(mPlugInpDispersion.GetValue());
  testhash.Hash(mPlugInpSeedLerp.GetValue());
  testhash.Hash(mPlugInpSmoothingRadius.GetValue());
  if (testhash != mVBHash) {
    ComputeVB(pTARG);
    mVBHash = testhash;
  }
  ////////////////////////////////////////////////////////////////
  struct AA16RenderCells : public AA16Render {
    virtual void DoRender(float left, float right, float top, float bot, Buffer& buf) {
      mPTX.GetTarget()->PushModColor(ork::fvec4::Red());
      fmtx4 mtxortho = mPTX.GetTarget()->MTXI()->Ortho(left, right, top, bot, 0.0f, 1.0f);
      mPTX.GetTarget()->MTXI()->PushPMatrix(mtxortho);
      for (int iw = 0; iw < 9; iw++) {
        int ix    = (iw % 3) - 1;
        int iy    = (iw / 3) - 1;
        fvec3 wpu = (ix > 0) ? wrapu : (ix < 0) ? -wrapu : fvec3::zero();
        fvec3 wpv = (iy > 0) ? wrapv : (iy < 0) ? -wrapv : fvec3::zero();
        fmtx4 mtx;
        mtx.setTranslation(wpu + wpv);
        mPTX.GetTarget()->MTXI()->PushMMatrix(mtx);
        mPTX.GetTarget()->GBI()->DrawPrimitive(&stdmat, mVW, ork::lev2::PrimitiveType::TRIANGLES);
        mPTX.GetTarget()->MTXI()->PopMMatrix();
      }
      mPTX.GetTarget()->MTXI()->PopPMatrix();
      mPTX.GetTarget()->PopModColor();
    }
    lev2::GfxMaterial3DSolid stdmat;
    ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16>& mVW;
    fvec3 wrapu;
    fvec3 wrapv;
    AA16RenderCells(ProcTex& ptx, Buffer& bo, ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16>& vw, int idimU, int idimV)
        : AA16Render(ptx, bo)
        , stdmat(ptx.GetTarget())
        , mVW(vw)
        , wrapu(float(idimU), 0.0f, 0.0f)
        , wrapv(0.0f, float(idimV), 0.0f) {
      stdmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
      stdmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      stdmat._rasterstate.SetCullTest(ork::lev2::ECullTest::OFF);
      stdmat._rasterstate.SetBlending(ork::lev2::Blending::OFF);
      stdmat._rasterstate.SetDepthTest(ork::lev2::EDepthTest::ALWAYS);
      stdmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(bo.miW)));

      mOrthoBoxXYWH = fvec4(0.0f, 0.0f, float(idimU), float(idimV));
    }
  };

  ////////////////////////////////////////////////////////////////

  AA16RenderCells renderer(ptex, buffer, mVW, miDimU, miDimV);
  renderer.Render(mbAA);

  pTARG->debugPopGroup();

  // MarkClean();
}

}} // namespace ork::proctex

///////////////////////////////////////////////////////////////////////////////

template bool ork::fplane3::ClipPoly<ork::proctex::CellPoly>(const ork::proctex::CellPoly& in, ork::proctex::CellPoly& out);
template bool ork::fplane3::ClipPoly<ork::proctex::CellPoly>(
    const ork::proctex::CellPoly& in,
    ork::proctex::CellPoly& fr,
    ork::proctex::CellPoly& bk);
