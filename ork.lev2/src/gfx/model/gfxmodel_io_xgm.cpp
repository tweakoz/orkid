#include <boost/filesystem.hpp>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include <ork/util/logger.h>
#include <ork/util/hexdump.inl>
#include <ork/kernel/memcpy.inl>
#include "../meshutil/assimp_util.inl"
using namespace std::literals;
namespace bfs = boost::filesystem;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_mioRXGM = logger()->createChannel("xgmREAD", fvec3(0.8, 0.8, 0.4), true);

bool XgmModel::_loadXGM(XgmModel* mdl, datablock_ptr_t datablock) {

  auto asset_load_req = mdl->_getLoadRequest();

  // printf("aaa: load _loadXGM datablock hash<%zx> length<%zu>\n", datablock->hash(), datablock->length() );

  //hexdumpbytes(datablock->_storage.data(), 64);

  constexpr int kVERSIONTAG = 0x01234567;
  bool rval                 = false;
  /////////////////////////////////////////////////////////////
  auto context = lev2::contextForCurrentThread();
  /////////////////////////////////////////////////////////////
  OrkHeapCheck();
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(datablock, allocator);
  OrkHeapCheck();
  /////////////////////////////////////////////////////////////
  if (chunkreader.IsOk()) {
    chunkfile::InputStream* HeaderStream    = chunkreader.GetStream("header");
    chunkfile::InputStream* ModelDataStream = chunkreader.GetStream("modeldata");
    chunkfile::InputStream* EmbTexStream    = chunkreader.GetStream("embtexmap");

    /////////////////////////////////////////////////////////
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
      int XGMVERSIONCODE = 0;
      HeaderStream->GetItem(XGMVERSIONCODE);
      HeaderStream->GetItem(inumjoints);
    }
    logchan_mioRXGM->log("XGM: inumjoints<%d>", inumjoints);
    /////////////////////////////////////////////////////////
    if (inumjoints) {
      mdl->_skeleton->resize(inumjoints);
      for (int ij = 0; ij < inumjoints; ij++) {
        int iskelindex     = 0;
        int iparentindex   = 0;
        int ijointname     = 0;
        int ijointpath     = 0;
        int ijointID       = 0;
        int ijointmatrix   = 0;
        int iinvrestmatrix = 0;
        int inodematrix    = 0;

        auto jprops                          = std::make_shared<XgmJointProperties>();
        mdl->_skeleton->_jointProperties[ij] = jprops;

        // read phase 0

        HeaderStream->GetItem(iskelindex);
        OrkAssert(ij == iskelindex);
        HeaderStream->GetItem(iparentindex);
        HeaderStream->GetItem(ijointname);
        HeaderStream->GetItem(ijointpath);
        HeaderStream->GetItem(ijointID);
        HeaderStream->GetItem(jprops->_numVerticesInfluenced);

        // read phase 1
        HeaderStream->GetItem(inodematrix);
        HeaderStream->GetItem(ijointmatrix);
        HeaderStream->GetItem(iinvrestmatrix);
        const char* pjntname = chunkreader.GetString(ijointname);
        const char* pjntpath = chunkreader.GetString(ijointpath);
        const char* pjntID   = chunkreader.GetString(ijointID);

        mdl->_skeleton->_jointIDS[ij] = pjntID;

        logchan_mioRXGM->log("XGM: joint index<%d> id<%s> name<%s>", ij, pjntID, pjntname);

        fmtx4 scalematrix;
        // scalematrix.compose(fvec3(0,0,0),fquat(),0.01f);

        std::string jname(pjntname);
        std::string jpath(pjntpath);
        std::string jid(pjntID);

        mdl->_skeleton->addJoint(iskelindex, iparentindex, jname, jpath, jid);
        ptstring.set(chunkreader.GetString(inodematrix));
        mdl->_skeleton->RefNodeMatrix(iskelindex) = scalematrix * PropType<fmtx4>::FromString(ptstring);
        ptstring.set(chunkreader.GetString(ijointmatrix));
        mdl->_skeleton->RefJointMatrix(iskelindex) = scalematrix * PropType<fmtx4>::FromString(ptstring);
        ptstring.set(chunkreader.GetString(iinvrestmatrix));
        auto bind_matrix                          = scalematrix * PropType<fmtx4>::FromString(ptstring);
        mdl->_skeleton->_bindMatrices[iskelindex] = bind_matrix;

        auto& decomp_out = mdl->_skeleton->_bindDecomps[iskelindex];

        bind_matrix.decompose(
            decomp_out._position,    //
            decomp_out._orientation, //
            decomp_out._scale.x);

        decomp_out._scale.y = decomp_out._scale.x;
        decomp_out._scale.z = decomp_out._scale.x;

        mdl->_skeleton->_inverseBindMatrices[iskelindex] = bind_matrix.inverse();
      }
    }
    ///////////////////////////////////
    // read flattened bones
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
      mdl->_skeleton->addBone(Bone);
    }
    if (inumbones) {
      mdl->_skeleton->miRootNode = (inumbones > 0) ? mdl->_skeleton->bone(0)._parentIndex : -1;
    }

    auto blocalpose                  = std::make_shared<XgmLocalPose>(mdl->_skeleton);
    mdl->_skeleton->_bind_local_pose = blocalpose;
    blocalpose->bindPose();
    blocalpose->blendPoses();
    blocalpose->concatenate();

    // mdl->_skeleton->dump();
    ///////////////////////////////////
    HeaderStream->GetItem(mdl->mBoundingCenter);
    HeaderStream->GetItem(mdl->mAABoundXYZ);
    HeaderStream->GetItem(mdl->mAABoundWHD);
    HeaderStream->GetItem(mdl->mBoundingRadius);

    logchan_mioRXGM->log("boundingCenter<%g %g %g>", mdl->mBoundingCenter.x, mdl->mBoundingCenter.y, mdl->mBoundingCenter.z);
    logchan_mioRXGM->log("boundingRadius<%g>", mdl->mBoundingRadius);
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
        auto texturedata   = EmbTexStream->GetCurrent();
        auto embtex        = new EmbeddedTexture;
        embtexmap[texname] = embtex;
        if (datasize) {
          auto texdatcopy = malloc(datasize);
          memcpy_fast(texdatcopy, texturedata, datasize);
          EmbTexStream->advance(datasize);
          embtex->_srcdata = texdatcopy;
        }
        embtex->_srcdatalen = datasize;
        logchan_mioRXGM->log("embtex<%zu:%s> datasiz<%zu> dataptr<%p>", i, texname, datasize, texturedata);
      }
    }

    ///////////////////////////////////
    chunkfile::XgmMaterialReaderContext materialread_ctx(chunkreader);
    materialread_ctx._varmap->mergeVars(mdl->_varmap);
    materialread_ctx._varmap->makeValueForKey<Context*>("gfxtarget") = context;
    materialread_ctx._inputStream                                    = HeaderStream;

    ///////////////////////////////////
    bool use_normalviz = false;
    if (auto try_override = mdl->_varmap.typedValueForKey<std::string>("override.shader.gbuf")) {
      const auto& override_sh = try_override.value();
      if (override_sh == "normalviz") {
        use_normalviz = true;
      }
    }

    xgmmaterial_override_map_ptr_t override_map;
    if (auto try_override_map = mdl->_varmap.typedValueForKey<xgmmaterial_override_map_ptr_t>("override.material.map")) {
      override_map = try_override_map.value();
    }

    ///////////////////////////////////
    for (int imat = 0; imat < inummats; imat++) {
      int iimat = 0, imatname = 0, imatclass = 0;
      HeaderStream->GetItem(iimat);
      OrkAssert(iimat == imat);
      HeaderStream->GetItem(imatname);
      HeaderStream->GetItem(imatclass);
      const char* pmatname      = chunkreader.GetString(imatname);
      const char* pmatclassname = chunkreader.GetString(imatclass);

      logchan_mioRXGM->log("pmatname<%s>", pmatname);
      logchan_mioRXGM->log("pmatclassname<%s>", pmatclassname);
      ork::object::ObjectClass* pmatclass = rtti::autocast(rtti::Class::FindClass(pmatclassname));
      logchan_mioRXGM->log("pmatclass<%p>", pmatclass);
      OrkAssert(pmatclass != nullptr);

      static const int kdefaulttranssortpass = 100;

      bool do_original_shader = true;
      if (override_map) {
        auto it = override_map->_mtl_map.find(pmatname);
        if (it != override_map->_mtl_map.end()) {
          auto pmat = it->second;
          pmat->mMaterialName = pmatname;
          mdl->AddMaterial(pmat);
          pmat->gpuInit(context);
          do_original_shader = false;
        }
      }

      ///////////////////////////////////////////////////////////

      if (do_original_shader) {
        ///////////////////////////////////////////////////////////
        // check xgm reader annotation
        ///////////////////////////////////////////////////////////

        auto anno = pmatclass->annotation("xgm.reader");
        if (auto as_reader = anno.tryAs<chunkfile::materialreader_t>()) {
          auto pmat = as_reader.value()(materialread_ctx);
          pmat->mMaterialName = pmatname;
          mdl->AddMaterial(pmat);
          //printf( "RUNREADER\n");
          pmat->gpuInit(context);
        }
        ///////////////////////////////////////////////////////////
        // material class not supported in XGM
        ///////////////////////////////////////////////////////////
        else {
          OrkAssert(false);
        }
      }
    }
    ///////////////////////////////////
    for (int imesh = 0; imesh < inummeshes; imesh++) {
      auto Mesh = std::make_shared<XgmMesh>();

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

        auto submesh = std::make_shared<XgmSubMesh>();
        Mesh->AddSubMesh(submesh);
        XgmSubMesh& xgm_sub_mesh = *submesh;

        int numclusters = 0;
        HeaderStream->GetItem(numclusters);

        HeaderStream->GetItem(imatname);
        const char* matname = chunkreader.GetString(imatname);

        //////////////////////////////

        for (int imat = 0; imat < mdl->miNumMaterials; imat++) {
          auto pmat = mdl->GetMaterial(imat);
          if (pmat->mMaterialName == matname) {
            xgm_sub_mesh._material = pmat;
          }
        }

        for (int ic = 0; ic < numclusters; ic++) {
          auto cluster = std::make_shared<XgmCluster>();
          xgm_sub_mesh._clusters.push_back(cluster);
          int iclusindex = -1;
          int inumbb     = -1;
          int ivboffset  = -1;
          int ivbnum     = -1;
          int ivbsize    = -1;
          fvec3 boxmin, boxmax;
          EVtxStreamFormat efmt;

          ////////////////////////////////////////////////////////////////////////
          HeaderStream->GetItem(iclusindex);
          OrkAssert(ic == iclusindex);
          int numprimgroups = 0;
          HeaderStream->GetItem(numprimgroups);
          HeaderStream->GetItem(inumbb);
          HeaderStream->GetItem<EVtxStreamFormat>(efmt);
          HeaderStream->GetItem(ivboffset);
          HeaderStream->GetItem(ivbnum);
          HeaderStream->GetItem(ivbsize);
          HeaderStream->GetItem(boxmin);
          HeaderStream->GetItem(boxmax);
          ////////////////////////////////////////////////////////////////////////
          cluster->mBoundingBox.SetMinMax(boxmin, boxmax);
          cluster->mBoundingSphere = Sphere(boxmin, boxmax);
          ////////////////////////////////////////////////////////////////////////
          // logchan_mioRXGM->log( "XGMLOAD vbfmt<%s> efmt<%d>d, vbfmt, int(efmt) );
          ////////////////////////////////////////////////////////////////////////
          cluster->_vertexBuffer = VertexBufferBase::CreateVertexBuffer(efmt, ivbnum, true);
          void* pverts           = (void*)(ModelDataStream->GetDataAt(ivboffset));
          int ivblen             = ivbnum * ivbsize;
          // logchan_mioRXGM->log("ReadVB NumVerts<%d> VtxSize<%d>d, ivbnum, pvb->GetVtxSize());
          void* poutverts = context->GBI()->LockVB(*cluster->_vertexBuffer.get(), 0, ivbnum); // ivblen );
          {

            if( asset_load_req and asset_load_req->_on_event ){
              asset_load_req->_on_event("beginCopyVertexBuffer"_crcu,nullptr);
            }
            memcpy_fast(poutverts, pverts, ivblen);
            if( asset_load_req and asset_load_req->_on_event ){
              asset_load_req->_on_event("endCopyVertexBuffer"_crcu,nullptr);
            }
            cluster->_vertexBuffer->SetNumVertices(ivbnum);
            if (efmt == EVtxStreamFormat::V12N12B12T8I4W4) {
              auto pv = (const SVtxV12N12B12T8I4W4*)pverts;
              for (int iv = 0; iv < ivbnum; iv++) {
                auto& v = pv[iv];
                auto& p = v.mPosition;
                auto& n = v.mNormal;
                OrkAssert(n.length() > 0.95);
                // logchan_mioRXGM->log( " iv<%d> pos<%f %f %f> bi<%08x> bw<%08x>d, iv, p.x, p.y, p.z, v.mBoneIndices,
                // v.mBoneWeights );
              }
            }
          }
          context->GBI()->UnLockVB(*cluster->_vertexBuffer.get());
          ////////////////////////////////////////////////////////////////////////
          for (int32_t ipg = 0; ipg < numprimgroups; ipg++) {
            auto newprimgroup = std::make_shared<XgmPrimGroup>();
            cluster->_primgroups.push_back(newprimgroup);
            int32_t ipgindex    = -1;
            int32_t ipgprimtype = -1;
            HeaderStream->GetItem<int32_t>(ipgindex);
            OrkAssert(ipgindex == ipg);

            HeaderStream->GetItem<PrimitiveType>(newprimgroup->mePrimType);
            HeaderStream->GetItem<int32_t>(newprimgroup->miNumIndices);

            int32_t idxdataoffset = -1;
            HeaderStream->GetItem<int32_t>(idxdataoffset);

            U16* pidx = (U16*)ModelDataStream->GetDataAt(idxdataoffset);

            auto pidxbuf = new StaticIndexBuffer<U16>(newprimgroup->miNumIndices);

            void* poutidx = (void*)context->GBI()->LockIB(*pidxbuf);
            { memcpy_fast(poutidx, pidx, newprimgroup->miNumIndices * sizeof(U16)); }
            context->GBI()->UnLockIB(*pidxbuf);

            newprimgroup->mpIndices = pidxbuf;
          }
          ////////////////////////////////////////////////////////////////////////
          cluster->_jointPaths.resize(inumbb);
          cluster->mJointSkelIndices.resize(inumbb);
          for (int ib = 0; ib < inumbb; ib++) {
            int ibindingindex = -1;
            int ibindingname  = -1;

            HeaderStream->GetItem(ibindingindex);
            HeaderStream->GetItem(ibindingname);

            const char* jointpath = chunkreader.GetString(ibindingname);
            auto itfind           = mdl->_skeleton->_jointsByPath.find(jointpath);

            if (itfind == mdl->_skeleton->_jointsByPath.end()) {
              logerrchannel()->log("\n\ncannot find joint<%s> in:", jointpath);
              for (auto it : mdl->_skeleton->_jointsByPath) {
                logerrchannel()->log("  %s", it.first.c_str());
              }
              OrkAssert(false);
            }
            int iskelindex                 = (*itfind).second;
            cluster->_jointPaths[ib]       = jointpath;
            cluster->mJointSkelIndices[ib] = iskelindex;
          }

          mdl->mbSkinned |= (inumbb > 0);

          // logchan_mioRXGM->log("mdl<%p> mbSkinned<%d>d, (void*) mdl, int(mdl->mbSkinned));
          ////////////////////////////////////////////////////////////////////////
        }
      }
    }
    rval = true;
  } // if( chunkreader.IsOk() )
  else {
    OrkAssert(false);
  }
  for (int i = 0; i < mdl->_skeleton->miNumJoints; i++) {
    int iparent = mdl->_skeleton->jointParent(i);
    if (iparent >= 0) {
      auto jprops = mdl->_skeleton->_jointProperties[iparent];
      jprops->_children.insert(i);
    }
  }
  // rval->_skeleton->dump();
  // mdl->dump();
  OrkHeapCheck();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
