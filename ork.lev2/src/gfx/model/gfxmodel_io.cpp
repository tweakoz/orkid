////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/environment.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/rtti/downcast.h>
#include <boost/filesystem.hpp>
#include <ork/kernel/datacache.h>
#include <ork/util/logger.h>
#include <ork/util/hexdump.inl>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/meshutil/meshutil.h>

namespace bfs = boost::filesystem;
namespace ork::meshutil {
datablock_ptr_t assimpToXgm(datablock_ptr_t inp_datablock,mesh_transformer_pipe_t mproc);
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

static bool FORCE_MODEL_REGEN() {
  return genviron.has("ORKID_LEV2_FORCE_MODEL_REGEN");
}
static bool ASSET_ENCRYPT_MODE() {
  return genviron.has("ORKID_ASSET_ENCRYPT_MODE");
}
static logchannel_ptr_t logchan_mioR = logger()->createChannel("gfxmodelIOREAD", fvec3(0.8, 0.8, 0.4), true);
static logchannel_ptr_t logchan_mioW = logger()->createChannel("gfxmodelIOWRITE", fvec3(0.8, 0.7, 0.4), true);
///////////////////////////////////////////////////////////////////////////////
bool SaveXGM(const AssetPath& Filename, const lev2::XgmModel* mdl) {

  // logchan_mioR->log("Writing Xgm<%p> to path<%s>d, (void*) mdl, Filename.c_str());
  auto datablock = writeXgmToDatablock(mdl);
  if (datablock) {
    ork::File outputfile(Filename, ork::EFM_WRITE);
    outputfile.Write(datablock->data(), datablock->length());
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

asset::loadrequest_ptr_t XgmModel::_getLoadRequest() {
  if (_asset) {
    return _asset->_load_request;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmModel::LoadUnManaged(XgmModel* mdl, const AssetPath& Filename, asset::vars_ptr_t vars) {

  if (not logchan_mioR->_enabled)
    logchan_mioR->_enabled = FORCE_MODEL_REGEN();

  bool rval        = false;
  auto ActualPath  = Filename.toAbsolute();
  mdl->msModelName = AddPooledString(Filename.c_str());
  /////////////////////
  // merge in asset vars
  /////////////////////
  if (vars) {
    mdl->_varmap.mergeVars(*vars);
  }
  /////////////////////
  auto load_req = mdl->_getLoadRequest();
  if (load_req) {
    load_req->incrementPartialLoadCount();
  }
  /////////////////////
  if (auto datablock = datablockFromFileAtPath(ActualPath)) {
    ///////////////////////////////////
    // annotate the datablock with some info
    //  that might be useful in loading the file
    ///////////////////////////////////
    auto actual_as_bfs = ActualPath.toBFS();
    auto base_dir      = actual_as_bfs.parent_path();
    OrkAssert(bfs::exists(actual_as_bfs));
    OrkAssert(bfs::is_regular_file(actual_as_bfs));
    OrkAssert(bfs::exists(base_dir));
    OrkAssert(bfs::is_directory(base_dir));
    datablock->_vars->makeValueForKey<std::string>("file-extension") = ActualPath.getExtension().c_str();
    datablock->_vars->makeValueForKey<bfs::path>("base-directory")   = base_dir;
    /////////////////////
    // merge in asset vars
    /////////////////////
    if (vars) {
      datablock->_vars->mergeVars(*vars);
    }
    ///////////////////////////////////
    rval = _loaderSelect(mdl, datablock);
  }
  if (load_req) {
    load_req->decrementPartialLoadCount();
  }
  return rval;
}

////////////////////////////////////////////////////////////

bool XgmModel::_loaderSelect(XgmModel* mdl, datablock_ptr_t datablock) {
  auto load_req = mdl->_getLoadRequest();
  DataBlockInputStream datablockstream(datablock);
  Char4 check_magic(datablockstream.getItem<uint32_t>());
  auto extension = datablock->_vars->typedValueForKey<std::string>("file-extension").value();
  if (extension == "gltf" or //
      extension == "dae" or  //
      extension == "fbx" or  //
      extension == "obj") {
    bool OK = _loadAssimp(mdl, datablock);
    if (load_req) {
      auto data                                    = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<std::string>("loader") = "_loadAssimp";
      if (OK) {
        load_req->_on_event("loadComplete"_crcu, data);
      } else {
        load_req->_on_event("loadFailed"_crcu, data);
      }
    }
    return OK;
  } else if (check_magic == Char4("chkf")) { // its a chunkfile
    bool OK = _loadXGM(mdl, datablock);
    if (load_req) {
      auto data                                    = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<std::string>("loader") = "_loadXGM";
      if (OK) {
        load_req->_on_event("loadComplete"_crcu, data);
      } else {
        load_req->_on_event("loadFailed"_crcu, data);
      }
    }
    return OK;
  } else if (check_magic == Char4("glTF")) { // its a glb (binary)
    if (ASSET_ENCRYPT_MODE()) {
      auto codec               = std::make_shared<DefaultEncryptionCodec>();
      auto encrypted_datablock = datablock->encrypt(codec);
      auto outpath             = file::Path::temp_dir() / "temp.orkemdl";
      ork::File outputfile(outpath, ork::EFM_WRITE);
      outputfile.Write(encrypted_datablock->data(), encrypted_datablock->length());
    }
    return _loadAssimp(mdl, datablock);
  } else if (datablock->is_ascii() and datablock->is_likely_json()) {
    datablock->zeroExtend();
    bool OK = _loadOrkScene(mdl, datablock);
    return OK;
  }
  printf("check_magic<%08x>\n", check_magic);
  if (check_magic.muVal32 == 0x736d656f) { // its encrypted
    printf("aaa: decrypting datablock hash<%zx> length<%zu>\n", datablock->hash(), datablock->length());
    uint32_t codecID = datablockstream.getItem<uint32_t>();
    auto& storage    = datablock->_storage;
    storage.erase(storage.begin(), storage.begin() + 8);
    printf("aaa: decrypting datablock rehash<%zx> relength<%zu>\n", datablock->hash(), datablock->length());
    auto codec               = encryptionCodecFactory(codecID);
    auto decrypted_datablock = datablock->decrypt(codec);
    hexdumpbytes(decrypted_datablock->_storage.data(), 64);
    printf("aaa: decrypted_datablock hash<%zx> length<%zu>\n", decrypted_datablock->hash(), decrypted_datablock->length());
    bool OK = _loadAssimp(mdl, decrypted_datablock);
    printf("assimp load status<%d>\n", int(OK));
    // OrkAssert(false);
    return OK;
  }

  return false;
}

////////////////////////////////////////////////////////////

bool XgmModel::_loadAssimp(XgmModel* mdl, datablock_ptr_t inp_datablock) {
  // printf("aaa: load assimp datablock hash<%zx> length<%zu>\n", inp_datablock->hash(), inp_datablock->length() );
  auto basehasher = DataBlock::createHasher();
  basehasher->accumulateString("assimp2xgm");

  basehasher->accumulateString("version-x042423");

  inp_datablock->accumlateHash(basehasher);
  /////////////////////////////////////
  // include asset vars as hash mutator
  //  because they may influence the loading mechanism
  /////////////////////////////////////
  auto meshpipe = meshutil::mesh_transformer_pipe_t();
  for (auto item : mdl->_varmap._themap) {
    const std::string& k = item.first;
    const rendervar_t& v = item.second;
    basehasher->accumulateString(k);
    if (auto as_str = v.tryAs<std::string>()) {
      basehasher->accumulateString(as_str.value());
      logchan_mioR->log("LOADASSIMP VAR<%s> <%s>", k.c_str(), as_str.value().c_str());
    } else if (auto as_bool = v.tryAs<bool>()) {
      basehasher->accumulateItem<bool>(as_bool.value());
    } else if (auto as_dbl = v.tryAs<double>()) {
      basehasher->accumulateItem<double>(as_dbl.value());
    } else {
      //OrkAssert(false);
    }
  }
  /////////////////////////////////////
  basehasher->finish();
  uint64_t hashkey   = basehasher->result();
  auto xgm_datablock = DataBlockCache::findDataBlock(hashkey);
  if (FORCE_MODEL_REGEN()) {
    xgm_datablock = nullptr;
  }
  logchan_mioR->log("xgm_datablock<%p>", (void*)xgm_datablock.get());
  if (not xgm_datablock) {
    xgm_datablock = meshutil::assimpToXgm(inp_datablock,meshpipe);
    DataBlockCache::setDataBlock(hashkey, xgm_datablock);
  }
  return _loadXGM(mdl, xgm_datablock);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

datablock_ptr_t writeXgmToDatablock(const lev2::XgmModel* mdl) {
  datablock_ptr_t out_datablock = std::make_shared<DataBlock>();

  lev2::ContextDummy DummyTarget;

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
  logchan_mioW->log("WriteXgm: VERSION<%d>", iVERSION);

  ///////////////////////////////////
  // write out joints
  ///////////////////////////////////

  const lev2::XgmSkeleton& skel = mdl->skeleton();

  int32_t inumjoints = skel.numJoints();

  HeaderStream->AddItem(inumjoints);

  int32_t istring;
  logchan_mioW->log("WriteXgm: numjoints<%d>", inumjoints);

  for (int32_t ij = 0; ij < inumjoints; ij++) {

    const std::string& JointName = skel._jointNAMES[ij];
    const std::string& JointPath = skel._jointPATHS[ij];
    const std::string& JointID   = skel._jointIDS[ij];
    int32_t JointParentIndex     = skel.jointParent(ij);
    const fmtx4& bind_matrix     = skel._bindMatrices[ij];
    const fmtx4& JointMatrix     = skel.RefJointMatrix(ij);
    const fmtx4& NodeMatrix      = skel.RefNodeMatrix(ij);

    // write phase 0
    HeaderStream->AddItem(ij);
    HeaderStream->AddItem(JointParentIndex);
    istring = chunkwriter.stringIndex(JointName.c_str());
    HeaderStream->AddItem(istring);
    istring = chunkwriter.stringIndex(JointPath.c_str());
    HeaderStream->AddItem(istring);
    istring = chunkwriter.stringIndex(JointID.c_str());
    HeaderStream->AddItem(istring);
    auto jprops = skel._jointProperties[ij];
    HeaderStream->AddItem(jprops->_numVerticesInfluenced);

    // write phase 1
    PropTypeString tstr;
    PropType<fmtx4>::ToString(NodeMatrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);

    PropType<fmtx4>::ToString(JointMatrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);

    PropType<fmtx4>::ToString(bind_matrix, tstr);
    istring = chunkwriter.stringIndex(tstr.c_str());
    HeaderStream->AddItem(istring);
  }

  ///////////////////////////////////
  // write out flattened bones
  ///////////////////////////////////

  int32_t inumbones = skel.numBones();

  HeaderStream->AddItem(inumbones);

  logchan_mioW->log("WriteXgm: numbones<%d>", inumbones);
  for (int32_t ib = 0; ib < inumbones; ib++) {
    const lev2::XgmBone& Bone = skel.bone(ib);

    HeaderStream->AddItem(ib);
    HeaderStream->AddItem(Bone._parentIndex);
    HeaderStream->AddItem(Bone._childIndex);
  }

  ///////////////////////////////////
  // basic model data
  ///////////////////////////////////

  int32_t inummeshes = mdl->numMeshes();
  int32_t inummats   = mdl->GetNumMaterials();

  logchan_mioW->log("WriteXgm: nummeshes<%d>", inummeshes);
  logchan_mioW->log("WriteXgm: nummtls<%d>", inummats);

  const fvec3& bc    = mdl->boundingCenter();
  float br           = mdl->GetBoundingRadius();
  const fvec3& bbxyz = mdl->GetBoundingAA_XYZ();
  const fvec3& bbwhd = mdl->boundingAA_WHD();

  HeaderStream->AddItem(bc.x);
  HeaderStream->AddItem(bc.y);
  HeaderStream->AddItem(bc.z);
  HeaderStream->AddItem(bbxyz.x);
  HeaderStream->AddItem(bbxyz.y);
  HeaderStream->AddItem(bbxyz.z);
  HeaderStream->AddItem(bbwhd.x);
  HeaderStream->AddItem(bbwhd.y);
  HeaderStream->AddItem(bbwhd.z);
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
      std::string texname = item.first;
      logchan_mioW->log("WriteXgm: writetex<%s>", texname.c_str());
      embtex_ptr_t ptex = item.second;
      int istring           = chunkwriter.stringIndex(texname.c_str());
      textureStream->AddItem<int>(istring);
      auto ddsblock = ptex->_ddsdestdatablock;
      if (ddsblock) {
        size_t blocksize = ddsblock->length();
        textureStream->AddItem<size_t>(blocksize);
        auto data = (const void*)ddsblock->data();
        textureStream->AddData(data, blocksize);
      } else {
        textureStream->AddItem<size_t>(0);
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////
  // materials
  ///////////////////////////////////////////////////////////////////////////////////////////

  for (int32_t imat = 0; imat < inummats; imat++) {
    auto pmat        = mdl->GetMaterial(imat);
    auto matclass    = pmat->GetClass();
    auto& writeranno = matclass->annotation("xgm.writer");

    HeaderStream->AddItem(imat);
    istring = chunkwriter.stringIndex(pmat->GetName().c_str());
    HeaderStream->AddItem(istring);

    rtti::Class* pclass    = pmat->GetClass();
    auto classname         = pclass->Name();
    const char* pclassname = classname.c_str();

    logchan_mioW->log("WriteXgm: material<%d> class<%s> name<%s>", imat, pclassname, pmat->GetName().c_str());
    istring = chunkwriter.stringIndex(classname.c_str());
    HeaderStream->AddItem(istring);

    logchan_mioW->log("Material Name<%s> Class<%s>", pmat->GetName().c_str(), classname.c_str());

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // new style material writer
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    if (auto as_writer = writeranno.tryAs<chunkfile::materialwriter_t>()) {
      chunkfile::XgmMaterialWriterContext mtlwctx(chunkwriter);
      mtlwctx._material     = pmat;
      mtlwctx._outputStream = HeaderStream;
      auto& writer          = as_writer.value();
      writer(mtlwctx);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // material class not supported for XGM
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    else {
      OrkAssert(false);
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////
  // meshes
  ///////////////////////////////////////////////////////////////////////////////////////////

  for (int32_t imesh = 0; imesh < inummeshes; imesh++) {
    const lev2::XgmMesh& Mesh = *mdl->mesh(imesh);

    int32_t inumsubmeshes = Mesh.numSubMeshes();

    HeaderStream->AddItem(imesh);
    istring = chunkwriter.stringIndex(Mesh.meshName().c_str());
    HeaderStream->AddItem(istring);
    HeaderStream->AddItem(inumsubmeshes);

    logchan_mioW->log("WriteXgm: mesh<%d:%s> numsubmeshes<%d>", imesh, Mesh.meshName().c_str(), inumsubmeshes);
    for (int32_t ics = 0; ics < inumsubmeshes; ics++) {
      const lev2::XgmSubMesh& xgm_sub_mesh = *Mesh.subMesh(ics);
      auto pmat                            = xgm_sub_mesh.GetMaterial();

      int32_t inumclus = xgm_sub_mesh.GetNumClusters();

      int32_t inumenabledclus = 0;

      for (int ic = 0; ic < inumclus; ic++) {
        auto cluster = xgm_sub_mesh.cluster(ic);
        auto VB      = cluster->_vertexBuffer;

        if (!VB)
          return nullptr;

        if (VB->GetNumVertices() > 0) {
          inumenabledclus++;
        } else {
          logchan_mioW->log(
              "WARNING: material<%s> cluster<%d> has a zero length vertex buffer, skipping", pmat->GetName().c_str(), ic);
        }
      }

      HeaderStream->AddItem(ics);
      HeaderStream->AddItem(inumenabledclus);

      logchan_mioW->log("WriteXgm:  submesh<%d> numenaclus<%d>", ics, inumenabledclus);
      ////////////////////////////////////////////////////////////
      istring = chunkwriter.stringIndex(pmat ? pmat->GetName().c_str() : "None");
      HeaderStream->AddItem(istring);
      ////////////////////////////////////////////////////////////
      for (int32_t ic = 0; ic < inumclus; ic++) {
        auto cluster              = xgm_sub_mesh.cluster(ic);
        auto VB                   = cluster->_vertexBuffer;
        const Sphere& clus_sphere = cluster->mBoundingSphere;
        const AABox& clus_box     = cluster->mBoundingBox;

        if (VB->GetNumVertices() == 0)
          continue;

        int32_t inumpg = cluster->numPrimGroups();
        int32_t inumjb = (int)cluster->numJointBindings();

        logchan_mioW->log("VB<%p> NumVerts<%d>", (void*)VB.get(), VB->GetNumVertices());
        logchan_mioW->log("clus<%d> numjb<%d>", ic, inumjb);

        int32_t ivbufoffset = ModelDataStream->GetSize();
        const u8* VBdata    = (const u8*)DummyTarget.GBI()->LockVB(*VB);
        OrkAssert(VBdata != 0);
        {

          int VBlen = VB->GetNumVertices() * VB->GetVtxSize();

          logchan_mioW->log("WriteVB VB<%p> NumVerts<%d> VtxSize<%d>", (void*)VB.get(), VB->GetNumVertices(), VB->GetVtxSize());

          HeaderStream->AddItem(ic);
          HeaderStream->AddItem(inumpg);
          HeaderStream->AddItem(inumjb);
          HeaderStream->AddItem<lev2::EVtxStreamFormat>(VB->GetStreamFormat());
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
          auto PG = cluster->primgroup(ipg);

          int32_t inumidx = PG->GetNumIndices();

          logchan_mioW->log("WritePG<%d> NumIndices<%d>", ipg, inumidx);

          HeaderStream->AddItem(ipg);
          HeaderStream->AddItem<PrimitiveType>(PG->GetPrimType());
          HeaderStream->AddItem<int32_t>(inumidx);
          HeaderStream->AddItem<int32_t>(ModelDataStream->GetSize());

          //////////////////////////////////////////////////
          U16* pidx = (U16*)DummyTarget.GBI()->LockIB(*PG->GetIndexBuffer()); //->GetDataPointer();
          OrkAssert(pidx != 0);
          for (int32_t ii = 0; ii < inumidx; ii++) {
            int32_t iv = int32_t(pidx[ii]);
            if (iv >= VB->GetNumVertices()) {
              logchan_mioW->log("index id<%d> val<%d> is > vertex count<%d>", ii, iv, VB->GetNumVertices());
            }
            OrkAssert(iv < VB->GetNumVertices());

            // swapbytes_dynamic<U16>( pidx[ii] );
          }
          DummyTarget.GBI()->UnLockIB(*PG->GetIndexBuffer());
          //////////////////////////////////////////////////

          ModelDataStream->Write((const unsigned char*)pidx, inumidx * sizeof(U16));
        }
        // write cluster bindings
        for (int32_t ij = 0; ij < inumjb; ij++) {
          const std::string& bound_path = cluster->jointBinding(ij);
          OrkAssert(bound_path != "");
          HeaderStream->AddItem(ij);
          istring = chunkwriter.stringIndex(bound_path.c_str());
          logchan_mioW->log("bound_path<%s> istring<%d>", bound_path.c_str(), istring);
          HeaderStream->AddItem(istring);
        }
      }
    }
  }
  chunkwriter.writeToDataBlock(out_datablock);
  printf("aaa: _saveXGM datablock hash<%zx> len<%zu>\n", out_datablock->hash(), out_datablock->length());
  // OrkAssert(false);
  return out_datablock;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
