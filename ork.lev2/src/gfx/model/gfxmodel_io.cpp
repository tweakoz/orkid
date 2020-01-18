////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/rtti/downcast.h>

#if defined(WII)
#include <ork/mem/wii_mem.h>
#endif

#if !defined(USE_XGM_FILES)
#include <miniork_tool/filter/gfx/collada/collada.h>
#endif

const bool kfidle_hack = false; // perhaps a bad export from maya...

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_XGM_FILES)

////////////////////////////////////////////////////////////
// account for bug in old XGM files (version 0)
// since EVTXSTREAMFMT_V12N12T16C4 was missing,
// old xgm files had bad enum strings
// => when loading old xgm files
//     and seeing EVTXSTREAMFMT_V12N12B12T8C4 or higher , subtract one
// we will add a xgm version string to the format
//  and attempt to make the PropTypeString for enums more robust
////////////////////////////////////////////////////////////

EVtxStreamFormat GetVersion0VertexStreamFormat(const char* fmtstr) {
  static bool gbINIT = true;
  static orkmap<std::string, EVtxStreamFormat> formatmap;
  if (gbINIT) {
    formatmap["EVTXSTREAMFMT_V16"]      = EVTXSTREAMFMT_V16;      // 0
    formatmap["EVTXSTREAMFMT_V4T4"]     = EVTXSTREAMFMT_V4T4;     // 1
    formatmap["EVTXSTREAMFMT_V4C4"]     = EVTXSTREAMFMT_V4C4;     // 2
    formatmap["EVTXSTREAMFMT_V4T4C4"]   = EVTXSTREAMFMT_V4T4C4;   // 3
    formatmap["EVTXSTREAMFMT_V12C4T16"] = EVTXSTREAMFMT_V12C4T16; // 4

    formatmap["EVTXSTREAMFMT_V12N6I1T4"] = EVTXSTREAMFMT_V12N6I1T4; // 5
    formatmap["EVTXSTREAMFMT_V12N6C2T4"] = EVTXSTREAMFMT_V12N6C2T4;

    formatmap["EVTXSTREAMFMT_V16T16C16"]   = EVTXSTREAMFMT_V16T16C16; // 7
    formatmap["EVTXSTREAMFMT_V12I4N12T8"]  = EVTXSTREAMFMT_V12I4N12T8;
    formatmap["EVTXSTREAMFMT_V12C4N6I2T8"] = EVTXSTREAMFMT_V12C4N6I2T8;
    formatmap["EVTXSTREAMFMT_V6I2C4N3T2"]  = EVTXSTREAMFMT_V6I2C4N3T2;
    formatmap["EVTXSTREAMFMT_V12I4N6W4T4"] = EVTXSTREAMFMT_V12I4N6W4T4; // 11

    formatmap["EVTXSTREAMFMT_V12N12T8I4W4"]    = EVTXSTREAMFMT_V12N12T8I4W4; // 12
    formatmap["EVTXSTREAMFMT_V12N12B12T8"]     = EVTXSTREAMFMT_V12N12B12T8;
    formatmap["EVTXSTREAMFMT_V12N12T16C4"]     = EVTXSTREAMFMT_V12N12B12T8; // uhoh ! << this was missing
    formatmap["EVTXSTREAMFMT_V12N12B12T8C4"]   = EVTXSTREAMFMT_V12N12T16C4; // 15
    formatmap["EVTXSTREAMFMT_V12N12B12T16"]    = EVTXSTREAMFMT_V12N12B12T8C4;
    formatmap["EVTXSTREAMFMT_V12N12B12T8I4W4"] = EVTXSTREAMFMT_V12N12B12T8I4W4; // 17

    formatmap["EVTXSTREAMFMT_MODELERRIGID"] = EVTXSTREAMFMT_MODELERRIGID; // 18

    formatmap["EVTXSTREAMFMT_XP_VCNT"]  = EVTXSTREAMFMT_MODELERRIGID; // 19
    formatmap["EVTXSTREAMFMT_XP_VCNTI"] = EVTXSTREAMFMT_END;
    formatmap["EVTXSTREAMFMT_END"]      = EVTXSTREAMFMT_END;
  }
  orkmap<std::string, EVtxStreamFormat>::const_iterator it = formatmap.find(fmtstr);
  EVtxStreamFormat eret                                    = EVTXSTREAMFMT_END;
  if (it != formatmap.end())
    eret = it->second;
  return eret;
}
////////////////////////////////////////////////////////////

bool XgmModel::LoadUnManaged(XgmModel* mdl, const AssetPath& Filename) {
  Context* pTARG               = GfxEnv::GetRef().loadingContext();
  bool rval                    = true;
  int XGMVERSIONCODE           = 0;
  static const int kVERSIONTAG = 0x01234567;
  /////////////////////////////////////////////////////////////
  AssetPath fnameext(Filename);
  fnameext.SetExtension("xgm");
  auto ActualPath = fnameext.ToAbsolute();
  printf("XgmModel: %s\n", ActualPath.c_str());

  /////////////////////////////////////////////////////////////
  OrkHeapCheck();
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(fnameext, "xgm", allocator);
  OrkHeapCheck();
  /////////////////////////////////////////////////////////////
  if (chunkreader.IsOk()) {
    chunkfile::InputStream* HeaderStream    = chunkreader.GetStream("header");
    chunkfile::InputStream* ModelDataStream = chunkreader.GetStream("modeldata");
    chunkfile::InputStream* EmbTexStream    = chunkreader.GetStream("embtexmap");

    /////////////////////////////////////////////////////////
    mdl->msModelName = AddPooledString(Filename.c_str());
    /////////////////////////////////////////////////////////
    mdl->mbSkinned = false;
    /////////////////////////////////////////////////////////
    PropTypeString ptstring;
    /////////////////////////////////////////////////////////
    int inumjoints = 0;
    HeaderStream->GetItem(inumjoints);
    /////////////////////////////////////////////////////////
    // test for version tag
    /////////////////////////////////////////////////////////
    if (inumjoints == kVERSIONTAG) {
      HeaderStream->GetItem(XGMVERSIONCODE);
      HeaderStream->GetItem(inumjoints);
    }
    /////////////////////////////////////////////////////////
    if (inumjoints) {
      mdl->mSkeleton.resize(inumjoints);
      for (int ib = 0; ib < inumjoints; ib++) {
        int iskelindex = 0, iparentindex = 0, ijointname = 0, ijointmatrix = 0, iinvrestmatrix = 0;
        int inodematrix = 0;
        HeaderStream->GetItem(iskelindex);
        OrkAssert(ib == iskelindex);
        HeaderStream->GetItem(iparentindex);
        HeaderStream->GetItem(ijointname);
        HeaderStream->GetItem(inodematrix);
        HeaderStream->GetItem(ijointmatrix);
        HeaderStream->GetItem(iinvrestmatrix);
        const char* pjntname = chunkreader.GetString(ijointname);

        fxstring<256> jnamp(pjntname);
        if (kfidle_hack) {
          jnamp.replace_in_place("f_idle_", "");
          printf("FIXUPJOINTNAME<%s:%s>\n", pjntname, jnamp.c_str());
        }
        mdl->mSkeleton.AddJoint(iskelindex, iparentindex, AddPooledString(jnamp.c_str()));
        ptstring.set(chunkreader.GetString(inodematrix));
        mdl->mSkeleton.RefNodeMatrix(iskelindex) = PropType<fmtx4>::FromString(ptstring);
        ptstring.set(chunkreader.GetString(ijointmatrix));
        mdl->mSkeleton.RefJointMatrix(iskelindex) = PropType<fmtx4>::FromString(ptstring);
        ptstring.set(chunkreader.GetString(iinvrestmatrix));
        mdl->mSkeleton.RefInverseBindMatrix(iskelindex) = PropType<fmtx4>::FromString(ptstring);
      }
    }
    ///////////////////////////////////
    // write out flattened bones
    ///////////////////////////////////
    int inumbones = 0;
    HeaderStream->GetItem(inumbones);
    for (int ib = 0; ib < inumbones; ib++) {
      int iib = 0;
      lev2::XgmBone Bone;
      HeaderStream->GetItem(iib);
      OrkAssert(iib == ib);
      HeaderStream->GetItem(Bone._parentIndex);
      HeaderStream->GetItem(Bone._childIndex);
      mdl->mSkeleton.addBone(Bone);
    }
    if (inumbones) {
      mdl->mSkeleton.miRootNode = (inumbones > 0) ? mdl->mSkeleton.bone(0)._parentIndex : -1;
    }
    // mdl->mSkeleton.dump();
    ///////////////////////////////////
    HeaderStream->GetItem(mdl->mBoundingCenter);
    HeaderStream->GetItem(mdl->mAABoundXYZ);
    HeaderStream->GetItem(mdl->mAABoundWHD);
    HeaderStream->GetItem(mdl->mBoundingRadius);
    // END HACK
    ///////////////////////////////////
    int inummeshes = 0, inummats = 0;
    HeaderStream->GetItem(mdl->miBonesPerCluster);
    HeaderStream->GetItem(inummeshes);
    HeaderStream->GetItem(inummats);
    ///////////////////////////////////
    mdl->mMeshes.reserve(inummeshes);
    ///////////////////////////////////
    // embedded textures
    ///////////////////////////////////
    auto& embtexmap = mdl->_varmap.makeValueForKey<embtexmap_t>("embtexmap");
    if (EmbTexStream) {
      size_t numembtex = 0;
      EmbTexStream->GetItem(numembtex);
      int itexname = 0;
      for (size_t i = 0; i < numembtex; i++) {
        EmbTexStream->GetItem(itexname);
        auto texname    = chunkreader.GetString(itexname);
        size_t datasize = 0;
        EmbTexStream->GetItem(datasize);
        auto texturedata = EmbTexStream->GetCurrent();
        auto texdatcopy  = malloc(datasize);
        memcpy(texdatcopy, texturedata, datasize);
        EmbTexStream->advance(datasize);
        auto embtex         = new EmbeddedTexture;
        embtex->_srcdata    = texdatcopy;
        embtex->_srcdatalen = datasize;
        embtexmap[texname]  = embtex;
        printf("embtex<%zu:%s> datasiz<%zu> dataptr<%p>\n", i, texname, datasize, texturedata);
      }
    }

    ///////////////////////////////////
    chunkfile::XgmMaterialReaderContext materialread_ctx(chunkreader);
    materialread_ctx._inputStream                                      = HeaderStream;
    materialread_ctx._varmap.makeValueForKey<Context*>("gfxtarget")    = pTARG;
    materialread_ctx._varmap.makeValueForKey<embtexmap_t>("embtexmap") = embtexmap;
    ///////////////////////////////////
    for (int imat = 0; imat < inummats; imat++) {
      int iimat = 0, imatname = 0, imatclass = 0;
      HeaderStream->GetItem(iimat);
      OrkAssert(iimat == imat);
      HeaderStream->GetItem(imatname);
      HeaderStream->GetItem(imatclass);
      const char* pmatname                = chunkreader.GetString(imatname);
      const char* pmatclassname           = chunkreader.GetString(imatclass);
      ork::object::ObjectClass* pmatclass = rtti::autocast(rtti::Class::FindClass(pmatclassname));

      lev2::GfxMaterial* pmat = 0;

      static const int kdefaulttranssortpass = 100;

      // printf("MODEL USEMATCLASS<%s>\n", pmatclassname);

      /////////////////////////////////////////////////////////////
      // wii (basic) material
      /////////////////////////////////////////////////////////////
      if (pmatclass == GfxMaterialWiiBasic::GetClassStatic()) {
        int ibastek    = -1;
        int iblendmode = -1;

        HeaderStream->GetItem(ibastek);

        const char* bastek = chunkreader.GetString(ibastek);

        // printf("MODEL USETEK<%s>\n", bastek);
        // assert(false);
        GfxMaterialWiiBasic* pbasmat = new GfxMaterialWiiBasic(bastek);
        pbasmat->Init(pTARG);
        pmat = pbasmat;
        pmat->SetName(AddPooledString(pmatname));
        HeaderStream->GetItem(iblendmode);
        const char* blendmodestring          = chunkreader.GetString(iblendmode);
        lev2::EBlending eblend               = PropType<lev2::EBlending>::FromString(blendmodestring);
        lev2::RenderQueueSortingData& rqdata = pbasmat->GetRenderQueueSortingData();

        if ((eblend != lev2::EBLENDING_OFF)) {
          rqdata.miSortingPass = kdefaulttranssortpass;
          pbasmat->_rasterstate.SetAlphaTest(EALPHATEST_GREATER, 0.0f);
        }
        pbasmat->_rasterstate.SetBlending(eblend);
      }
      /////////////////////////////////////////////////////////////
      // minimal solid material
      /////////////////////////////////////////////////////////////
      else if (pmatclass == lev2::GfxMaterial3DSolid::GetClassStatic()) {
        lev2::GfxMaterial3DSolid* pmatsld = new lev2::GfxMaterial3DSolid;
        int imode;
        fvec4 color;
        //				float fr, fg, fb, fa;
        HeaderStream->GetItem(imode);
        HeaderStream->GetItem(color);

        // printf( "READCOLOR %f %f %f %f\n", color.GetX(), color.GetY(), color.GetZ(), color.GetW() );

        pmatsld->Init(pTARG);
        pmatsld->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_INTERNAL_COLOR);
        pmatsld->SetColor(color);
        pmat = pmatsld;
        pmat->SetName(AddPooledString(pmatname));
      }
      /////////////////////////////////////////////////////////////
      // data driven FX material
      /////////////////////////////////////////////////////////////
      else if (pmatclass == lev2::GfxMaterialFx::GetClassStatic()) {
        lev2::GfxMaterialFx* pmatfx          = rtti::autocast(pmatclass->CreateObject());
        lev2::RenderQueueSortingData& rqdata = pmatfx->GetRenderQueueSortingData();
        rqdata.miSortingPass                 = 0; // kdefaulttranssortpass;
        pmat                                 = pmatfx;
        pmat->SetName(AddPooledString(pmatname));
        int iparamcount = -1;
        HeaderStream->GetItem(iparamcount);
        for (int ic = 0; ic < iparamcount; ic++) {
          int ipt = -1;
          int ipn = -1;
          int ipv = -1;
          HeaderStream->GetItem(ipt);
          HeaderStream->GetItem(ipn);
          HeaderStream->GetItem(ipv);
          const char* paramname = chunkreader.GetString(ipn);
          const char* paramval  = chunkreader.GetString(ipv);
          // orkprintf( "READXGM paramtype<%d> paramname<%s> paramval<%s>\n", ipt, paramname, paramval );
          EPropType ept                 = EPropType(ipt);
          GfxMaterialFxParamBase* param = 0;
          switch (ept) {
            case EPROPTYPE_VEC2REAL: {
              GfxMaterialFxParamArtist<fvec2>* paramf = new GfxMaterialFxParamArtist<fvec2>;
              paramf->mValue                          = PropType<fvec2>::FromString(paramval);
              param                                   = paramf;
              break;
            }
            case EPROPTYPE_VEC3FLOAT: {
              GfxMaterialFxParamArtist<fvec3>* paramf = new GfxMaterialFxParamArtist<fvec3>;
              paramf->mValue                          = PropType<fvec3>::FromString(paramval);
              param                                   = paramf;
              break;
            }
            case EPROPTYPE_VEC4REAL: {
              GfxMaterialFxParamArtist<fvec4>* paramf = new GfxMaterialFxParamArtist<fvec4>;
              paramf->mValue                          = PropType<fvec4>::FromString(paramval);
              param                                   = paramf;
              break;
            }
            case EPROPTYPE_MAT44REAL: {
              GfxMaterialFxParamArtist<fmtx4>* paramf = new GfxMaterialFxParamArtist<fmtx4>;
              paramf->mValue                          = PropType<fmtx4>::FromString(paramval);
              param                                   = paramf;
              break;
            }
            case EPROPTYPE_REAL: {
              GfxMaterialFxParamArtist<float>* paramf = new GfxMaterialFxParamArtist<float>;
              paramf->mValue                          = PropType<float>::FromString(paramval);
              param                                   = paramf;
              orkprintf("ModelIO::LoadFloatParam mdl<> param<%s> val<%s>\n", paramname, paramval);
              break;
            }
            case EPROPTYPE_S32: {
              ////////////////////////////////////////////////////////
              // read artist supplied renderqueue sorting key
              ////////////////////////////////////////////////////////
              int ival = PropType<int>::FromString(paramval);
              if (strcmp(paramname, "ork_rqsort") == 0) {
                rqdata.miSortingOffset = ival;
              } else if (strcmp(paramname, "ork_rqsort_pass") == 0) {
                rqdata.miSortingPass = ival;
              } else {
                GfxMaterialFxParamArtist<int>* paramf = new GfxMaterialFxParamArtist<int>;
                paramf->mValue                        = ival;
                param                                 = paramf;
              }
              break;
            }
            case EPROPTYPE_SAMPLER: {
              GfxMaterialFxParamArtist<lev2::Texture*>* paramf = new GfxMaterialFxParamArtist<lev2::Texture*>;

              AssetPath texname(paramval);
              const char* ptexnam = texname.c_str();
              printf("texname<%s>\n", ptexnam);
              Texture* ptex(NULL);
              if (0 != strcmp(texname.c_str(), "None")) {
                ork::lev2::TextureAsset* ptexa = asset::AssetManager<TextureAsset>::Create(texname.c_str());
                ptex                           = ptexa ? ptexa->GetTexture() : 0;
              }
              // orkprintf( "ModelIO::LoadTexture mdl<%s> tex<%s> ptex<%p>\n", "", texname.c_str(), ptex );
              paramf->mValue = ptex;
              param          = paramf;
              break;
            }
            case EPROPTYPE_STRING: {
              GfxMaterialFxParamArtist<std::string>* paramstr = new GfxMaterialFxParamArtist<std::string>;
              paramstr->mValue                                = paramval;
              param                                           = paramstr;
              param->SetBindable(false);
              break;
            }
            default:
              OrkAssert(false);
              break;
          }
          if (param) {
            param->GetRecord()._name = paramname;
            pmatfx->AddParameter(param);
          }
        }
        // pmat->Init( pTARG );
      } else {

        ///////////////////////////////////////////////////////////
        // check xgm reader annotation
        ///////////////////////////////////////////////////////////

        auto anno = pmatclass->annotation("xgm.reader");
        if (auto as_reader = anno.TryAs<chunkfile::materialreader_t>()) {
          pmat = as_reader.value()(materialread_ctx);
          pmat->SetName(AddPooledString(pmatname));
        }

        ///////////////////////////////////////////////////////////
        // material class not supported in XGM
        ///////////////////////////////////////////////////////////
        else {
          OrkAssert(false);
        }
      }

      mdl->AddMaterial(pmat);

      for (int idest = int(ETEXDEST_AMBIENT); idest != int(ETEXDEST_END); idest++) {
        int itexdest = -1;
        HeaderStream->GetItem(itexdest);
        const char* texdest    = chunkreader.GetString(itexdest);
        ETextureDest TexDest   = PropType<ETextureDest>::FromString(texdest);
        TextureContext& TexCtx = pmat->GetTexture(TexDest);
        int itexname;
        HeaderStream->GetItem(itexname);
        AssetPath texname(chunkreader.GetString(itexname));
        Texture* ptex(NULL);
        if (0 != strcmp(texname.c_str(), "None")) {
          // orkprintf( "Loadtexture<%s>\n", texname.c_str());
          texname.SetUrlBase(Filename.GetUrlBase().c_str());
          texname.SetFolder(Filename.GetFolder(ork::file::Path::EPATHTYPE_NATIVE).c_str());

          ptex = asset::AssetManager<TextureAsset>::Create(texname.c_str())->GetTexture();
        }
        pmat->SetTexture(TexDest, ptex);
        HeaderStream->GetItem(TexCtx.mfRepeatU);
        HeaderStream->GetItem(TexCtx.mfRepeatV);
      }
    }
    for (int imesh = 0; imesh < inummeshes; imesh++) {
      XgmMesh* Mesh = new XgmMesh;

      int itestmeshindex    = -1;
      int itestmeshname     = -1;
      int imeshnummats      = -1;
      int imeshnumsubmeshes = -1;

      HeaderStream->GetItem(itestmeshindex);
      OrkAssert(itestmeshindex == imesh);

      HeaderStream->GetItem(itestmeshname);
      const char* MeshName  = chunkreader.GetString(itestmeshname);
      PoolString MeshNamePS = AddPooledString(MeshName);
      Mesh->SetMeshName(MeshNamePS);
      mdl->mMeshes.AddSorted(MeshNamePS, Mesh);

      HeaderStream->GetItem(imeshnumsubmeshes);

      Mesh->ReserveSubMeshes(imeshnumsubmeshes);

      for (int ics = 0; ics < imeshnumsubmeshes; ics++) {
        int itestclussetindex = -1, imatname = -1;
        HeaderStream->GetItem(itestclussetindex);
        OrkAssert(ics == itestclussetindex);

        XgmSubMesh* submesh = new XgmSubMesh;
        Mesh->AddSubMesh(submesh);
        XgmSubMesh& CS = *submesh;

        HeaderStream->GetItem(CS.miNumClusters);

        int ilightmapname;
        int ivtxlitflg;

        HeaderStream->GetItem(imatname);
        HeaderStream->GetItem(ilightmapname);
        HeaderStream->GetItem(ivtxlitflg);

        const char* matname    = chunkreader.GetString(imatname);
        submesh->mLightMapPath = file::Path(chunkreader.GetString(ilightmapname));

        //////////////////////////////
        // vertex lit or lightmapped ?
        //////////////////////////////
        if (ivtxlitflg) {
          submesh->mbVertexLit = true;
        }
        //////////////////////////////
        else if (submesh->mLightMapPath.length()) {
          if (FileEnv::DoesFileExist(submesh->mLightMapPath)) {
            ork::lev2::TextureAsset* plmtexa = asset::AssetManager<TextureAsset>::Create(submesh->mLightMapPath.c_str());
            submesh->mLightMap               = (plmtexa == 0) ? 0 : plmtexa->GetTexture();
          }
        }
        //////////////////////////////

        for (int imat = 0; imat < mdl->miNumMaterials; imat++) {
          GfxMaterial* pmat = mdl->GetMaterial(imat);
          if (strcmp(pmat->GetName().c_str(), matname) == 0) {
            CS.mpMaterial = pmat;
          }
        }

        CS.mpClusters = new XgmCluster[CS.miNumClusters];
        for (int ic = 0; ic < CS.miNumClusters; ic++) {
          int iclusindex = -1;
          int inumbb     = -1;
          int ivbformat  = -1;
          int ivboffset  = -1;
          int ivbnum     = -1;
          int ivbsize    = -1;
          fvec3 boxmin, boxmax;

          ////////////////////////////////////////////////////////////////////////
          HeaderStream->GetItem(iclusindex);
          OrkAssert(ic == iclusindex);
          XgmCluster& Clus = CS.cluster(ic);
          HeaderStream->GetItem(Clus.miNumPrimGroups);
          HeaderStream->GetItem(inumbb);
          HeaderStream->GetItem(ivbformat);
          HeaderStream->GetItem(ivboffset);
          HeaderStream->GetItem(ivbnum);
          HeaderStream->GetItem(ivbsize);
          HeaderStream->GetItem(boxmin);
          HeaderStream->GetItem(boxmax);
          ////////////////////////////////////////////////////////////////////////
          Clus.mBoundingBox.SetMinMax(boxmin, boxmax);
          Clus.mBoundingSphere = Sphere(boxmin, boxmax);
          ////////////////////////////////////////////////////////////////////////
          const char* vbfmt     = chunkreader.GetString(ivbformat);
          EVtxStreamFormat efmt = PropType<EVtxStreamFormat>::FromString(vbfmt);
          // printf( "XGMLOAD vbfmt<%s> efmt<%d>\n", vbfmt, int(efmt) );
          ////////////////////////////////////////////////////////////////////////
          // fix a bug in old files
          if (0 == XGMVERSIONCODE) {
            efmt = GetVersion0VertexStreamFormat(vbfmt);
            // printf( "XGMLOAD(V0FIX) new efmt<%d>\n", efmt );
          }
          ////////////////////////////////////////////////////////////////////////
          // lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
          VertexBufferBase* pvb = VertexBufferBase::CreateVertexBuffer(efmt, ivbnum, true);
          void* pverts          = (void*)(ModelDataStream->GetDataAt(ivboffset));
          int ivblen            = ivbnum * ivbsize;

          // printf("ReadVB NumVerts<%d> VtxSize<%d>\n", ivbnum, pvb->GetVtxSize());
          void* poutverts = pTARG->GBI()->LockVB(*pvb, 0, ivbnum); // ivblen );
          {
            memcpy(poutverts, pverts, ivblen);
            pvb->SetNumVertices(ivbnum);
            if (efmt == EVTXSTREAMFMT_V12N12B12T8I4W4) {
              auto pv = (const SVtxV12N12B12T8I4W4*)pverts;
              for (int iv = 0; iv < ivbnum; iv++) {
                auto& v = pv[iv];
                auto& p = v.mPosition;
                auto& n = v.mNormal;
                OrkAssert(n.length() > 0.95);
                // printf( " iv<%d> pos<%f %f %f> bi<%08x> bw<%08x>\n", iv, p.GetX(), p.GetY(), p.GetZ(), v.mBoneIndices,
                // v.mBoneWeights );
              }
            }
          }
          pTARG->GBI()->UnLockVB(*pvb);
          // lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
          Clus._vertexBuffer = pvb;
          ////////////////////////////////////////////////////////////////////////
          Clus.mpPrimGroups = new XgmPrimGroup[Clus.miNumPrimGroups];
          for (int32_t ipg = 0; ipg < Clus.miNumPrimGroups; ipg++) {
            int32_t ipgindex    = -1;
            int32_t ipgprimtype = -1;
            HeaderStream->GetItem<int32_t>(ipgindex);
            OrkAssert(ipgindex == ipg);

            XgmPrimGroup& PG = Clus.RefPrimGroup(ipg);
            HeaderStream->GetItem<int32_t>(ipgprimtype);
            const char* primtype = chunkreader.GetString(ipgprimtype);
            PG.mePrimType        = PropType<EPrimitiveType>::FromString(primtype);
            HeaderStream->GetItem<int32_t>(PG.miNumIndices);

            int32_t idxdataoffset = -1;
            HeaderStream->GetItem<int32_t>(idxdataoffset);

            U16* pidx = (U16*)ModelDataStream->GetDataAt(idxdataoffset);

            auto pidxbuf = new StaticIndexBuffer<U16>(PG.miNumIndices);

            void* poutidx = (void*)pTARG->GBI()->LockIB(*pidxbuf);
            {
              // TODO: Make 16-bit indices a policy
              if (PG.miNumIndices > 0xFFFF)
                orkerrorlog(
                    "WARNING: <%s> Wii cannot have num indices larger than 65535: MeshName=%s, MatName=%s\n",
                    Filename.c_str(),
                    MeshName,
                    matname);

              memcpy(poutidx, pidx, PG.miNumIndices * sizeof(U16));
            }
            pTARG->GBI()->UnLockIB(*pidxbuf);

            PG.mpIndices = pidxbuf;
          }
          ////////////////////////////////////////////////////////////////////////
          Clus.mJoints.resize(inumbb);
          Clus.mJointSkelIndices.resize(inumbb);
          for (int ib = 0; ib < inumbb; ib++) {
            int ibindingindex = -1;
            int ibindingname  = -1;

            HeaderStream->GetItem(ibindingindex);
            HeaderStream->GetItem(ibindingname);

            const char* jointname = chunkreader.GetString(ibindingname);
            fxstring<256> jnamp(jointname);
            if (kfidle_hack) {
              jnamp.replace_in_place("f_idle_", "");
              printf("FIXUPJOINTNAME<%s:%s>\n", jointname, jnamp.c_str());
            }
            PoolString JointNameIndex                      = FindPooledString(jnamp.c_str());
            orklut<PoolString, int>::const_iterator itfind = mdl->mSkeleton.mmJointNameMap.find(JointNameIndex);

            OrkAssert(itfind != mdl->mSkeleton.mmJointNameMap.end());
            int iskelindex             = (*itfind).second;
            Clus.mJoints[ib]           = AddPooledString(jnamp.c_str());
            Clus.mJointSkelIndices[ib] = iskelindex;
          }

          mdl->mbSkinned |= (inumbb > 0);

          printf("mdl<%p> mbSkinned<%d>\n", mdl, int(mdl->mbSkinned));
          ////////////////////////////////////////////////////////////////////////
        }
      }
    }
  } // if( chunkreader.IsOk() )

  // rval->mSkeleton.dump();
  // mdl->dump();
  OrkHeapCheck();
  return rval;
}

#else
XgmModel* XgmModel::Load(const std::string& Filename) {
  if (ork::tool::CColladaModel* colladaModel = ork::tool::CColladaModel::Load(Filename)) {
    colladaModel->mXgmModel.mpColladaModel = colladaModel;
    colladaModel->mXgmModel.msModelName    = Filename;
    return &colladaModel->mXgmModel;
  }

  return NULL;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool SaveXGM(const AssetPath& Filename, const lev2::XgmModel* mdl) {
  printf("Writing Xgm<%p> to path<%s>\n", mdl, Filename.c_str());
  EndianContext* pendianctx = 0;

  bool bwii   = (0 != strstr(Filename.c_str(), "wii"));
  bool bxb360 = (0 != strstr(Filename.c_str(), "xb360"));

  lev2::ContextDummy DummyTarget;

  if (bwii || bxb360) {
    pendianctx          = new EndianContext;
    pendianctx->mendian = ork::EENDIAN_BIG;
  }

  ///////////////////////////////////
  chunkfile::Writer chunkwriter("xgm");

  ///////////////////////////////////
  chunkfile::OutputStream* HeaderStream    = chunkwriter.AddStream("header");
  chunkfile::OutputStream* ModelDataStream = chunkwriter.AddStream("modeldata");

  ///////////////////////////////////
  // write out new VERSION code
  int32_t iVERSIONTAG = 0x01234567;
  int32_t iVERSION    = 1;
  HeaderStream->AddItem(iVERSIONTAG);
  HeaderStream->AddItem(iVERSION);
  printf("WriteXgm<%s> VERSION<%d>\n", Filename.c_str(), iVERSION);
  ///////////////////////////////////
  // write out Joints

  const lev2::XgmSkeleton& skel = mdl->skeleton();

  int32_t inumjoints = skel.numJoints();

  HeaderStream->AddItem(inumjoints);

  int32_t istring;
  printf("WriteXgm<%s> numjoints<%d>\n", Filename.c_str(), inumjoints);

  for (int32_t ib = 0; ib < inumjoints; ib++) {
    const PoolString& JointName = skel.GetJointName(ib);
    int32_t JointParentIndex    = skel.GetJointParent(ib);
    const fmtx4& InvRestMatrix  = skel.RefInverseBindMatrix(ib);
    const fmtx4& JointMatrix    = skel.RefJointMatrix(ib);
    const fmtx4& NodeMatrix     = skel.RefNodeMatrix(ib);

    HeaderStream->AddItem(ib);
    HeaderStream->AddItem(JointParentIndex);
    istring = chunkwriter.stringIndex(JointName.c_str());
    HeaderStream->AddItem(istring);

    PropTypeString tstr;
    PropType<fmtx4>::ToString(NodeMatrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);

    PropType<fmtx4>::ToString(JointMatrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);

    PropType<fmtx4>::ToString(InvRestMatrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);
  }

  ///////////////////////////////////
  // write out flattened bones

  int32_t inumbones = skel.numBones();

  HeaderStream->AddItem(inumbones);

  printf("WriteXgm<%s> numbones<%d>\n", Filename.c_str(), inumbones);
  for (int32_t ib = 0; ib < inumbones; ib++) {
    const lev2::XgmBone& Bone = skel.bone(ib);

    HeaderStream->AddItem(ib);
    HeaderStream->AddItem(Bone._parentIndex);
    HeaderStream->AddItem(Bone._childIndex);
  }

  ///////////////////////////////////

  int32_t inummeshes = mdl->numMeshes();
  int32_t inummats   = mdl->GetNumMaterials();

  printf("WriteXgm<%s> nummeshes<%d>\n", Filename.c_str(), inummeshes);
  printf("WriteXgm<%s> nummtls<%d>\n", Filename.c_str(), inummats);

  const fvec3& bc    = mdl->boundingCenter();
  float br           = mdl->GetBoundingRadius();
  const fvec3& bbxyz = mdl->GetBoundingAA_XYZ();
  const fvec3& bbwhd = mdl->boundingAA_WHD();

  HeaderStream->AddItem(bc.GetX());
  HeaderStream->AddItem(bc.GetY());
  HeaderStream->AddItem(bc.GetZ());
  HeaderStream->AddItem(bbxyz.GetX());
  HeaderStream->AddItem(bbxyz.GetY());
  HeaderStream->AddItem(bbxyz.GetZ());
  HeaderStream->AddItem(bbwhd.GetX());
  HeaderStream->AddItem(bbwhd.GetY());
  HeaderStream->AddItem(bbwhd.GetZ());
  HeaderStream->AddItem(br);

  HeaderStream->AddItem(mdl->GetBonesPerCluster());
  HeaderStream->AddItem(inummeshes);
  HeaderStream->AddItem(inummats);

  std::set<std::string> ParameterIgnoreSet;
  ParameterIgnoreSet.insert("binMembership");
  ParameterIgnoreSet.insert("colorSource");
  ParameterIgnoreSet.insert("texCoordSource");
  ParameterIgnoreSet.insert("uniformParameters");
  ParameterIgnoreSet.insert("varyingParameters");

  ///////////////////////////////////////////////////////////////////////////////////////////
  // embedded textures chunk
  ///////////////////////////////////////////////////////////////////////////////////////////

  if (auto as_embtexmap = mdl->_varmap.typedValueForKey<embtexmap_t>("embtexmap")) {
    auto& embtexmap    = as_embtexmap.value();
    auto textureStream = chunkwriter.AddStream("embtexmap");
    textureStream->AddItem<size_t>(embtexmap.size());
    for (auto item : embtexmap) {
      std::string texname   = item.first;
      EmbeddedTexture* ptex = item.second;
      int istring           = chunkwriter.stringIndex(texname.c_str());
      textureStream->AddItem<int>(istring);
      auto ddsblock    = ptex->_ddsdestdatablock;
      size_t blocksize = ddsblock->length();
      textureStream->AddItem<size_t>(blocksize);
      auto data = (const void*)ddsblock->data();
      textureStream->AddData(data, blocksize);
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////

  for (int32_t imat = 0; imat < inummats; imat++) {
    const lev2::GfxMaterial* pmat = mdl->GetMaterial(imat);
    auto matclass                 = pmat->GetClass();
    auto& writeranno              = matclass->annotation("xgm.writer");

    HeaderStream->AddItem(imat);
    istring = chunkwriter.stringIndex(pmat->GetName().c_str());
    HeaderStream->AddItem(istring);

    rtti::Class* pclass         = pmat->GetClass();
    const PoolString& classname = pclass->Name();
    const char* pclassname      = classname.c_str();

    printf("WriteXgm<%s> material<%d> class<%s> name<%s>\n", Filename.c_str(), imat, pclassname, pmat->GetName().c_str());
    istring = chunkwriter.stringIndex(classname.c_str());
    HeaderStream->AddItem(istring);

    printf("Material Name<%s> Class<%s>\n", pmat->GetName().c_str(), classname.c_str());

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // new style material writer
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    if (auto as_writer = writeranno.TryAs<chunkfile::materialwriter_t>()) {
      chunkfile::XgmMaterialWriterContext mtlwctx(chunkwriter);
      mtlwctx._material     = pmat;
      mtlwctx._outputStream = HeaderStream;
      auto& writer          = as_writer.value();
      writer(mtlwctx);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // basic materials (fixed, simple materials
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    else if (pmat->GetClass()->IsSubclassOf(lev2::GfxMaterialWiiBasic::GetClassStatic())) {
      auto pbasmat = rtti::safe_downcast<const lev2::GfxMaterialWiiBasic*>(pmat);
      istring      = chunkwriter.stringIndex(pbasmat->GetBasicTechName().c_str());
      HeaderStream->AddItem(istring);

      lev2::EBlending eblend = pbasmat->_rasterstate.GetBlending();

      PropTypeString BlendModeString;
      PropType<lev2::EBlending>::ToString(eblend, BlendModeString);

      istring = chunkwriter.stringIndex(BlendModeString.c_str());
      HeaderStream->AddItem(istring);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // solid material
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    else if (pmat->GetClass()->IsSubclassOf(lev2::GfxMaterial3DSolid::GetClassStatic())) {
      const lev2::GfxMaterial3DSolid* pbasmat = rtti::safe_downcast<const lev2::GfxMaterial3DSolid*>(pmat);

      int32_t imode    = int(pbasmat->GetColorMode());
      const fvec4& clr = pbasmat->GetColor();

      HeaderStream->AddItem(imode);

      HeaderStream->AddItem(clr);

    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // fx materials (data driven materials)
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    else if (pmat->GetClass()->IsSubclassOf(lev2::GfxMaterialFx::GetClassStatic())) {
      const lev2::GfxMaterialFx* pfxmat                               = rtti::safe_downcast<const lev2::GfxMaterialFx*>(pmat);
      const lev2::GfxMaterialFxEffectInstance& fxinst                 = pfxmat->GetEffectInstance();
      const orklut<std::string, lev2::GfxMaterialFxParamBase*>& parms = fxinst.mParameterInstances;

      int32_t iparamcount = 0;

      for (orklut<std::string, lev2::GfxMaterialFxParamBase*>::const_iterator itf = parms.begin(); itf != parms.end(); itf++) {
        const std::string& paramname              = itf->first;
        const lev2::GfxMaterialFxParamBase* pbase = itf->second;

        bool bignore = ParameterIgnoreSet.find(paramname) != ParameterIgnoreSet.end();

        if (false == bignore) {
          iparamcount++;
        }
      }
      HeaderStream->AddItem(iparamcount);

      for (orklut<std::string, lev2::GfxMaterialFxParamBase*>::const_iterator itf = parms.begin(); itf != parms.end(); itf++) {
        const std::string& paramname              = itf->first;
        const lev2::GfxMaterialFxParamBase* pbase = itf->second;

        bool bignore = ParameterIgnoreSet.find(paramname) != ParameterIgnoreSet.end();

        if (false == bignore) {
          EPropType etype = pbase->GetRecord().meParameterType;

          std::string valstr = pbase->GetValueString();

          printf("SaveXGM paramtype<%d> paramname<%s> valstr<%s>\n", int(etype), paramname.c_str(), valstr.c_str());

          const lev2::GfxMaterialFxParamArtist<lev2::Texture*>* ptexparam = rtti::autocast(pbase);

          if (ptexparam) {
            const char* ptexnam = pbase->GetInitString().c_str();
            std::string tmpstr(ptexnam);
            printf("texname<%s>\n", ptexnam);
#ifdef WIN32
            std::string::size_type loc = tmpstr.find("data\\src\\");
#else
            std::string::size_type loc = tmpstr.find("data/src/");
#endif
            if (loc == std::string::npos) {
              orkerrorlog("ERROR: Output texture path is outside of 'data\\src\\'! (%s)\n", tmpstr.c_str());
              return false;
            }
            tmpstr = std::string("data://") + tmpstr.substr(loc + 9);
            for (std::string::size_type i = 0; i < tmpstr.length(); i++)
              if (tmpstr[i] == '\\')
                tmpstr[i] = '/';
            file::Path AsPath(tmpstr.c_str());
            AsPath.SetExtension("");
            valstr = AsPath.c_str();

            printf("SaveXGM paramtype<%d> paramname<%s> valstr<%s>\n", int(etype), paramname.c_str(), valstr.c_str());
          }

          HeaderStream->AddItem(int32_t(etype));
          istring = chunkwriter.stringIndex(paramname.c_str());
          HeaderStream->AddItem(istring);
          istring = chunkwriter.stringIndex(valstr.c_str());
          HeaderStream->AddItem(istring);
        }
      }

    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // material class not supported for XGM
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    else {
      OrkAssert(false);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    for (int32_t idest = int(lev2::ETEXDEST_AMBIENT); idest != int(lev2::ETEXDEST_END); idest++) {
      lev2::ETextureDest edest = lev2::ETextureDest(idest);

      const lev2::TextureContext& TexCtx = pmat->GetTexture(edest);

      PropTypeString tstr;
      PropType<lev2::ETextureDest>::ToString(edest, tstr);
      std::string TexDest = tstr.c_str();

      std::string TexName = "None";
      if (TexCtx.mpTexture) {
        auto tname = TexCtx.mpTexture->_varmap.typedValueForKey<std::string>("abspath").value();
        ork::AssetPath pth(tname.c_str());
        pth.SetExtension("");
        pth.SetUrlBase("");
        pth.SetFolder("");
        TexName = pth.c_str();
      }

      if (TexName != "None") {
        // orkprintf( " WRITING slot<%s> texname<%s>\n", tstr.c_str(), TexName.c_str() );
      }

      istring = chunkwriter.stringIndex(TexDest.c_str());
      HeaderStream->AddItem(istring);
      istring = chunkwriter.stringIndex(TexName.c_str());
      HeaderStream->AddItem(istring);
      HeaderStream->AddItem(TexCtx.mfRepeatU);
      HeaderStream->AddItem(TexCtx.mfRepeatV);
    }
  }

  for (int32_t imesh = 0; imesh < inummeshes; imesh++) {
    const lev2::XgmMesh& Mesh = *mdl->mesh(imesh);

    int32_t inumsubmeshes = Mesh.numSubMeshes();

    HeaderStream->AddItem(imesh);
    istring = chunkwriter.stringIndex(Mesh.meshName().c_str());
    HeaderStream->AddItem(istring);
    HeaderStream->AddItem(inumsubmeshes);

    printf("WriteXgm<%s> mesh<%d:%s> numsubmeshes<%d>\n", Filename.c_str(), imesh, Mesh.meshName().c_str(), inumsubmeshes);
    for (int32_t ics = 0; ics < inumsubmeshes; ics++) {
      const lev2::XgmSubMesh& CS    = *Mesh.subMesh(ics);
      const lev2::GfxMaterial* pmat = CS.GetMaterial();

      int32_t inumclus = CS.GetNumClusters();

      int32_t inumenabledclus = 0;

      for (int ic = 0; ic < inumclus; ic++) {
        const lev2::XgmCluster& Clus     = CS.cluster(ic);
        const lev2::VertexBufferBase* VB = Clus._vertexBuffer;

        if (!VB)
          return false;

        if (VB->GetNumVertices() > 0) {
          inumenabledclus++;
        } else {
          orkprintf("WARNING: material<%s> cluster<%d> has a zero length vertex buffer, skipping\n", pmat->GetName().c_str(), ic);
        }
      }

      HeaderStream->AddItem(ics);
      HeaderStream->AddItem(inumenabledclus);

      printf("WriteXgm<%s>  submesh<%d> numenaclus<%d>\n", Filename.c_str(), ics, inumenabledclus);
      ////////////////////////////////////////////////////////////
      istring = chunkwriter.stringIndex(pmat ? pmat->GetName().c_str() : "None");
      HeaderStream->AddItem(istring);
      ////////////////////////////////////////////////////////////
      const file::Path& LightMapPath = CS.mLightMapPath;
      istring                        = chunkwriter.stringIndex(LightMapPath.c_str());
      HeaderStream->AddItem(istring);
      ////////////////////////////////////////////////////////////
      int32_t ivtxlitflg = 0;
      if (CS.mbVertexLit)
        ivtxlitflg = 1;
      HeaderStream->AddItem(ivtxlitflg);
      ////////////////////////////////////////////////////////////
      for (int32_t ic = 0; ic < inumclus; ic++) {
        const lev2::XgmCluster& Clus     = CS.cluster(ic);
        const lev2::VertexBufferBase* VB = Clus._vertexBuffer;
        lev2::VertexBufferBase* VBNC     = const_cast<lev2::VertexBufferBase*>(VB);
        const Sphere& clus_sphere        = Clus.mBoundingSphere;
        const AABox& clus_box            = Clus.mBoundingBox;

        if (VB->GetNumVertices() == 0)
          continue;

        int32_t inumpg = Clus.GetNumPrimGroups();
        int32_t inumjb = (int)Clus.GetNumJointBindings();

        printf("clus<%d> numjb<%d>\n", ic, inumjb);
        PropTypeString tstr;
        PropType<lev2::EVtxStreamFormat>::ToString(VB->GetStreamFormat(), tstr);
        std::string VertexFmt = tstr.c_str();

        int32_t ivbufoffset = ModelDataStream->GetSize();
        const u8* VBdata    = (const u8*)DummyTarget.GBI()->LockVB(*VB);
        OrkAssert(VBdata != 0);
        {

          int VBlen = VB->GetNumVertices() * VB->GetVtxSize();

          printf("WriteVB NumVerts<%d> VtxSize<%d>\n", VB->GetNumVertices(), VB->GetVtxSize());

          HeaderStream->AddItem(ic);
          HeaderStream->AddItem(inumpg);
          HeaderStream->AddItem(inumjb);

          istring = chunkwriter.stringIndex(VertexFmt.c_str());
          HeaderStream->AddItem(istring);
          HeaderStream->AddItem(ivbufoffset);
          HeaderStream->AddItem(VB->GetNumVertices());
          HeaderStream->AddItem(VB->GetVtxSize());

          HeaderStream->AddItem(clus_box.Min());
          HeaderStream->AddItem(clus_box.Max());

          // VBNC->EndianSwap();

          ModelDataStream->Write(VBdata, VBlen);
        }
        DummyTarget.GBI()->UnLockVB(*VB);

        for (int32_t ipg = 0; ipg < inumpg; ipg++) {
          const lev2::XgmPrimGroup& PG = Clus.RefPrimGroup(ipg);

          PropType<lev2::EPrimitiveType>::ToString(PG.GetPrimType(), tstr);
          std::string PrimType = tstr.c_str();

          int32_t inumidx = PG.GetNumIndices();

          printf("WritePG<%d> NumIndices<%d>\n", ipg, inumidx);

          HeaderStream->AddItem(ipg);
          istring = chunkwriter.stringIndex(PrimType.c_str());
          HeaderStream->AddItem<int32_t>(istring);
          HeaderStream->AddItem<int32_t>(inumidx);
          HeaderStream->AddItem<int32_t>(ModelDataStream->GetSize());

          //////////////////////////////////////////////////
          U16* pidx = (U16*)DummyTarget.GBI()->LockIB(*PG.GetIndexBuffer()); //->GetDataPointer();
          OrkAssert(pidx != 0);
          for (int32_t ii = 0; ii < inumidx; ii++) {
            int32_t iv = int32_t(pidx[ii]);
            if (iv >= VB->GetNumVertices()) {
              orkprintf("index id<%d> val<%d> is > vertex count<%d>\n", ii, iv, VB->GetNumVertices());
            }
            OrkAssert(iv < VB->GetNumVertices());

            // swapbytes_dynamic<U16>( pidx[ii] );
          }
          DummyTarget.GBI()->UnLockIB(*PG.GetIndexBuffer());
          //////////////////////////////////////////////////

          ModelDataStream->Write((const unsigned char*)pidx, inumidx * sizeof(U16));
        }

        for (int32_t ij = 0; ij < inumjb; ij++) {
          const PoolString& bound = Clus.GetJointBinding(ij);
          HeaderStream->AddItem(ij);
          istring = chunkwriter.stringIndex(bound.c_str());
          HeaderStream->AddItem(istring);
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////

  // file::Path outpath = Filename;
  //	outpath.SetExtension( "xgm" );
  chunkwriter.WriteToFile(Filename);

  ////////////////////////////////////////////////////////////////////////////////////

  if (pendianctx) {
    delete pendianctx;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
