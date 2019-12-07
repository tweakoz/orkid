///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/cfs.inl>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/filter.h>
#include <ork/kernel/spawner.h>

///////////////////////////////////////////////////////////////////////////////
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <assimp/material.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/meshutil/clusterizer.h>

#include <ork/lev2/gfx/material_pbr.inl>

INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::GLB_XGM_Filter, "GLB_XGM_Filter");

///////////////////////////////////////////////////////////////////////////////
namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void get_bounding_box_for_node(const aiScene* scene, const aiNode* nd, aiVector3D& min, aiVector3D& max, aiMatrix4x4& trafo) {
  aiMatrix4x4 prev;
  unsigned int n = 0, t;

  prev = trafo;
  aiMultiplyMatrix4(&trafo, &nd->mTransformation);

  for (; n < nd->mNumMeshes; ++n) {
    const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
    for (t = 0; t < mesh->mNumVertices; ++t) {

      aiVector3D tmp = mesh->mVertices[t];
      aiTransformVecByMatrix4(&tmp, &trafo);

      min.x = std::min(min.x, tmp.x);
      min.y = std::min(min.y, tmp.y);
      min.z = std::min(min.z, tmp.z);

      max.x = std::max(max.x, tmp.x);
      max.y = std::max(max.y, tmp.y);
      max.z = std::max(max.z, tmp.z);
    }
  }

  for (n = 0; n < nd->mNumChildren; ++n) {
    get_bounding_box_for_node(scene, nd->mChildren[n], min, max, trafo);
  }
  trafo = prev;
}

///////////////////////////////////////////////////////////////////////////////

struct GltfMaterial {
  std::string _name;
  std::string _metallicAndRoughnessMap;
  std::string _colormap;
  std::string _normalmap;
  std::string _amboccmap;
  std::string _emissivemap;
};

typedef std::map<int, GltfMaterial*> gltfmaterialmap_t;

///////////////////////////////////////////////////////////////////////////////

void toolmesh::readFromAssimp(const file::Path& BasePath, tool::DaeReadOpts& readopts) {

  ork::file::Path GlbPath = BasePath;
  // GlbPath.SetExtension("glb");

  auto& embtexmap = _varmap.makeValueForKey<lev2::embtexmap_t>("embtexmap");

  auto scene = aiImportFile(GlbPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
  if (scene) {
    aiVector3D scene_min, scene_max, scene_center;
    aiMatrix4x4 identity;
    aiIdentityMatrix4(&identity);
    scene_min.x = scene_min.y = scene_min.z = 1e10f;
    scene_max.x = scene_max.y = scene_max.z = -1e10f;
    get_bounding_box_for_node(scene, scene->mRootNode, scene_min, scene_max, identity);

    scene_center.x = (scene_min.x + scene_max.x) / 2.0f;
    scene_center.y = (scene_min.y + scene_max.y) / 2.0f;
    scene_center.z = (scene_min.z + scene_max.z) / 2.0f;

    //////////////////////////////////////////////
    // parse embedded textures
    //////////////////////////////////////////////

    printf( "NumTex<%d>\n", scene->mNumTextures );

    for (int i = 0; i < scene->mNumTextures; i++) {
      auto texture        = scene->mTextures[i];
      std::string fmt     = (const char*)texture->achFormatHint;
      std::string texname = (const char*)texture->mFilename.data;
      bool embedded       = false;
      if (texname.length() == 0) {
        texname  = FormatString("*%d", i);
        embedded = true;
      }

      auto embtex     = new lev2::EmbeddedTexture;
      embtex->_format = fmt;
      embtex->_name   = texname;

      if (fmt == "png" or fmt == "jpg") {
        int filelen         = texture->mWidth;
        auto data           = (const void*)texture->pcData;
        embtex->_srcdatalen = filelen;
        embtex->_srcdata    = data;
        embtex->fetchDDSdata();
        embtexmap[texname] = embtex;
      } else if (fmt == "rgba8888" or fmt == "argb8888") {
        int w                 = texture->mWidth;
        int h                 = texture->mHeight;
        const aiTexel* texels = texture->pcData;
      } else {
        assert(false);
      }
    }

    //////////////////////////////////////////////
    // parse materials
    //////////////////////////////////////////////

    gltfmaterialmap_t materialmap;

    for (int i = 0; i < scene->mNumMaterials; i++) {
      auto material = scene->mMaterials[i];

      auto outmtl    = new GltfMaterial;
      materialmap[i] = outmtl;

      std::string material_name;

      aiColor4D color;
      aiString string;
      if (AI_SUCCESS == aiGetMaterialString(material, AI_MATKEY_NAME, &string)) {
        printf("has name\n");
        material_name = (const char*)string.data;
        outmtl->_name = material_name;
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color)) {
        printf("has_uniform_diffuse\n");
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &color)) {
        printf("has_uniform_specular\n");
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &color)) {
        printf("has_uniform_ambient\n");
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &color)) {
        printf("has_uniform_emissive\n");
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_colormap = (const char*)string.data;
        auto it           = embtexmap.find(outmtl->_colormap);
        auto tex          = (it != embtexmap.end()) ? it->second : (lev2::EmbeddedTexture*)nullptr;
        printf("has_pbr_colormap<%s> tex<%p>\n", outmtl->_colormap.c_str(), tex);
      }
      if (AI_SUCCESS == aiGetMaterialTexture(material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &string)) {
        outmtl->_metallicAndRoughnessMap = (const char*)string.data;
        auto it                          = embtexmap.find(outmtl->_metallicAndRoughnessMap);
        auto tex                         = (it != embtexmap.end()) ? it->second : (lev2::EmbeddedTexture*)nullptr;
        printf("has_pbr_MetallicAndRoughnessMap<%s> tex<%p>\n", outmtl->_metallicAndRoughnessMap.c_str(), tex);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_NORMALS, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_normalmap = (const char*)string.data;
        auto it            = embtexmap.find(outmtl->_normalmap);
        auto tex           = (it != embtexmap.end()) ? it->second : (lev2::EmbeddedTexture*)nullptr;
        printf("has_pbr_normalmap<%s> tex<%p>\n", outmtl->_normalmap.c_str(), tex);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_AMBIENT, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_amboccmap = (const char*)string.data;
        auto it            = embtexmap.find(outmtl->_amboccmap);
        auto tex           = (it != embtexmap.end()) ? it->second : (lev2::EmbeddedTexture*)nullptr;
        printf("has_pbr_amboccmap<%s> tex<%p>", outmtl->_amboccmap.c_str(), tex);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_EMISSIVE, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_emissivemap = (const char*)string.data;
        auto it              = embtexmap.find(outmtl->_emissivemap);
        auto tex             = (it != embtexmap.end()) ? it->second : (lev2::EmbeddedTexture*)nullptr;
        printf("has_pbr_emissivemap<%s> tex<%p> \n", outmtl->_emissivemap.c_str(), tex);
      }
    }

    //////////////////////////////////////////////
    // parse nodes
    //////////////////////////////////////////////

    std::queue<aiNode*> nodestack;
    nodestack.push(scene->mRootNode);

    while (not nodestack.empty()) {

      auto n = nodestack.front();
      nodestack.pop();

      //////////////////////////////////////////////
      // visit node
      //////////////////////////////////////////////

      aiMatrix4x4 mtx = n->mTransformation;
      for (int i = 0; i < n->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[n->mMeshes[i]];
        /////////////////////////////////////////////
        // query which input data is available
        /////////////////////////////////////////////
        bool has_normals    = mesh->mNormals != nullptr;
        bool has_tangents   = mesh->mTangents != nullptr;
        bool has_bitangents = mesh->mBitangents != nullptr;
        bool has_colors     = mesh->mColors[0] != nullptr;
        bool has_uvs        = mesh->mTextureCoords[0] != nullptr;
        bool has_bones      = mesh->mNumBones != 0;
        /////////////////////////////////////////////
        const char* name = mesh->mName.data;
        /////////////////////////////////////////////
        GltfMaterial* outmtl = materialmap[mesh->mMaterialIndex];
        /////////////////////////////////////////////
        // merge geometry
        /////////////////////////////////////////////
        auto& out_submesh = MergeSubMesh(name);
        auto& mtlset      = out_submesh.typedAnnotation<std::set<int>>("materialset");
        mtlset.insert(mesh->mMaterialIndex);
        auto& mtlref = out_submesh.typedAnnotation<GltfMaterial*>("gltfmaterial");
        mtlref       = outmtl;
        /////////////////////////////////////////////
        MeshUtil::vertex muverts[4];
        for (int t = 0; t < mesh->mNumFaces; ++t) {
          const aiFace* face = &mesh->mFaces[t];
          bool is_triangle   = (face->mNumIndices == 3);
          if (is_triangle) {
            for (int facevert_index = 0; facevert_index < 3; facevert_index++) {
              int index                 = face->mIndices[facevert_index];
              const auto& v             = mesh->mVertices[index];
              const auto& n             = mesh->mNormals[index];
              const auto& uv            = (mesh->mTextureCoords[0])[index];
              const auto& b             = (mesh->mBitangents)[index];
              auto& muvtx               = muverts[facevert_index];
              muvtx.mPos                = fvec3(v.x, v.y, v.z);
              muvtx.mNrm                = fvec3(n.x, n.y, n.z);
              muvtx.mCol[0]             = fvec4(1, 1, 1, 1);
              muvtx.mUV[0].mMapTexCoord = fvec2(uv.x, uv.y);
              muvtx.mUV[0].mMapBiNormal = fvec3(b.x, b.y, b.z);
            }
            int outpoly_indices[3] = {-1, -1, -1};
            outpoly_indices[0]     = out_submesh.MergeVertex(muverts[0]);
            outpoly_indices[1]     = out_submesh.MergeVertex(muverts[1]);
            outpoly_indices[2]     = out_submesh.MergeVertex(muverts[2]);
            poly ply(outpoly_indices, 3);
            out_submesh.MergePoly(ply);
          } else {
            printf("non triangle\n");
          }
        }
        /////////////////////////////////////////////
        // stats
        /////////////////////////////////////////////
        // int meshout_numtris = out_submesh.GetNumPolys(3);
        // int meshout_numquads = out_submesh.GetNumPolys(4);
        // int meshout_numverts = out_submesh.RefVertexPool().GetNumVertices();
        // printf( "meshout_numtris<%d>\n", meshout_numtris );
        // printf( "meshout_numquads<%d>\n", meshout_numquads );
        // printf( "meshout_numverts<%d>\n", meshout_numverts );
        /////////////////////////////////////////////
      }

      //////////////////////////////////////////////
      // enqueue children
      //////////////////////////////////////////////

      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

GLB_XGM_Filter::GLB_XGM_Filter() {
}

///////////////////////////////////////////////////////////////////////////////

void GLB_XGM_Filter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

template <typename ClusterizerType> void clusterizeToolMeshToXgmMesh(const toolmesh& inp_model, ork::lev2::XgmModel& out_model) {

  auto& inp_embtexmap = inp_model._varmap.typedValueForKey<lev2::embtexmap_t>("embtexmap").value();
  auto& out_embtexmap = out_model._varmap.makeValueForKey<lev2::embtexmap_t>("embtexmap") = inp_embtexmap;

  out_model.ReserveMeshes(inp_model.RefSubMeshLut().size());
  ork::lev2::XgmMesh* out_mesh = new ork::lev2::XgmMesh;
  out_mesh->SetMeshName("Mesh1"_pool);
  out_model.AddMesh("Mesh1"_pool, out_mesh);

  auto VertexFormat = ork::lev2::EVTXSTREAMFMT_V12N12B12T16;

  struct SubRec {
    submesh* _toolsub                    = nullptr;
    ToolMaterialGroup* _toolmgrp         = nullptr;
    XgmClusterizer* _clusterizer         = nullptr;
    ork::lev2::PBRMaterial* _pbrmaterial = nullptr;
  };

  typedef std::vector<SubRec> xgmsubvect_t;
  typedef std::map<GltfMaterial*, xgmsubvect_t> mtl2submap_t;
  typedef std::map<GltfMaterial*, ToolMaterialGroup*> mtl2mtlmap_t;

  mtl2submap_t mtlsubmap;
  mtl2mtlmap_t mtlmtlmap;

  int subindex = 0;
  for (auto item : inp_model.RefSubMeshLut()) {
    subindex++;
    submesh* inp_submesh = item.second;
    auto& mtlset         = inp_submesh->typedAnnotation<std::set<int>>("materialset");
    auto gltfmtl         = inp_submesh->typedAnnotation<GltfMaterial*>("gltfmaterial");
    assert(mtlset.size() == 1); // assimp does 1 material per submesh

    auto mtlout = new ork::lev2::PBRMaterial();
    mtlout->setTextureBaseName(FormatString("material%d", subindex));
    mtlout->_colorMapName                  = gltfmtl->_colormap;
    mtlout->_normalMapName                 = gltfmtl->_normalmap;
    mtlout->_amboccMapName                 = gltfmtl->_amboccmap;
    mtlout->_roughMapName                  = gltfmtl->_metallicAndRoughnessMap;
    mtlout->_metalMapName                  = gltfmtl->_metallicAndRoughnessMap;
    mtlout->_metalicRoughnessSingleTexture = true;
    out_model.AddMaterial(mtlout);

    auto clusterizer                 = new ClusterizerType;
    ToolMaterialGroup* materialGroup = nullptr;
    auto it                          = mtlmtlmap.find(gltfmtl);
    if (it == mtlmtlmap.end()) {
      materialGroup                  = new ToolMaterialGroup;
      materialGroup->meMaterialClass = ToolMaterialGroup::EMATCLASS_PBR;
      materialGroup->SetClusterizer(clusterizer);
      materialGroup->mMeshConfigurationFlags.mbSkinned = false;
      materialGroup->meVtxFormat                       = VertexFormat;
      mtlmtlmap[gltfmtl]                               = materialGroup;
    } else
      materialGroup = it->second;

    XgmClusterTri clustertri;
    clusterizer->Begin();

    const auto& vertexpool = inp_submesh->RefVertexPool();
    const auto& polys      = inp_submesh->RefPolys();
    for (const auto& poly : polys) {
      assert(poly.GetNumSides() == 3);
      for (int i = 0; i < 3; i++)
        clustertri._vertex[i] = vertexpool.GetVertex(poly.GetVertexID(i));
      clusterizer->AddTriangle(clustertri, materialGroup);
    }

    clusterizer->End();

    ///////////////////////////////////////

    SubRec srec;
    srec._pbrmaterial = mtlout;
    srec._toolsub     = inp_submesh;
    srec._toolmgrp    = materialGroup;
    srec._clusterizer = clusterizer;
    mtlsubmap[gltfmtl].push_back(srec);

    ///////////////////////////////////////
  }

  //////////////////////////////////////////////////////////////////
  size_t count_subs = 0;
  for (auto item : mtlsubmap) {
    GltfMaterial* gltfm = item.first;
    auto& subvect       = item.second;
    for (auto& subrec : subvect) {
      count_subs++;
    }
  }
  //////////////////////////////////////////////////////////////////

  out_mesh->ReserveSubMeshes(count_subs);
  subindex = 0;

  for (auto item : mtlsubmap) {
    GltfMaterial* gltfm = item.first;
    auto& subvect       = item.second;
    for (auto& subrec : subvect) {
      auto pbr_material = subrec._pbrmaterial;
      auto clusterizer  = subrec._clusterizer;

      auto xgm_submesh        = new ork::lev2::XgmSubMesh;
      xgm_submesh->mpMaterial = pbr_material;
      out_mesh->AddSubMesh(xgm_submesh);
      subindex++;

      int inumclus = clusterizer->GetNumClusters();
      xgm_submesh->miNumClusters = inumclus;
      xgm_submesh->mpClusters = new lev2::XgmCluster[ inumclus ];
      for (int icluster = 0; icluster < inumclus; icluster++) {
        auto clusterbuilder      = dynamic_cast<XgmRigidClusterBuilder*>(clusterizer->GetCluster(icluster));
        const auto& tool_submesh = clusterbuilder->_submesh;
        clusterbuilder->buildVertexBuffer(VertexFormat);

		lev2::XgmCluster & XgmClus = xgm_submesh->mpClusters[ icluster ];
		buildTriStripXgmCluster( XgmClus, clusterbuilder );

		//int inumclusjoints = XgmClus.mJoints.size();
		//for( int ib=0; ib<inumclusjoints; ib++ )
		//{
		//	const PoolString JointName = XgmClus.mJoints[ ib ];
		//	orklut<PoolString,int>::const_iterator itfind = mXgmModel.RefSkel().mmJointNameMap.find( JointName );
		//	int iskelindex = (*itfind).second;
		//	XgmClus.mJointSkelIndices.push_back(iskelindex);
		//}

      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool GLB_XGM_Filter::ConvertAsset(const tokenlist& toklist) {
  ork::tool::FilterOptMap options;
  options.SetDefault("-dice", "false");
  options.SetDefault("-dicedim", "128.0f");
  options.SetDefault("-in", "yo");
  options.SetDefault("-out", "yo");
  options.SetOptions(toklist);
  const std::string inf  = options.GetOption("-in")->GetValue();
  const std::string outf = options.GetOption("-out")->GetValue();

  bool bDICE = options.GetOption("-dice")->GetValue() == "true";
  bool brval = false;

  OldSchool::SetGlobalStringVariable("StripJoinPolicy", "true");

  ///////////////////////////////////////////////////

  tool::ColladaExportPolicy policy;
  policy.mDDSInputOnly          = true; // TODO
  policy.mUnits                 = tool::UNITS_METER;
  policy.mSkinPolicy.mWeighting = tool::ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W4;
  policy.miNumBonesPerCluster   = 32;
  policy.mColladaInpName        = inf;
  policy.mColladaOutName        = outf;
  policy.mDicingPolicy.SetPolicy(bDICE ? tool::ColladaDicingPolicy::ECTP_DICE : tool::ColladaDicingPolicy::ECTP_DONT_DICE);
  policy.mTriangulation.SetPolicy(tool::ColladaTriangulationPolicy::ECTP_TRIANGULATE);

  ////////////////////////////////////////////////////////////////
  // PC vertex formats supported
  policy.mAvailableVertexFormats.add(lev2::EVTXSTREAMFMT_V12N12T8I4W4);    // PC basic skinned
  policy.mAvailableVertexFormats.add(lev2::EVTXSTREAMFMT_V12N12B12T8I4W4); // PC 1 tanspace skinned
  policy.mAvailableVertexFormats.add(lev2::EVTXSTREAMFMT_V12N12B12T8C4);   // PC 1 tanspace unskinned
  policy.mAvailableVertexFormats.add(lev2::EVTXSTREAMFMT_V12N12B12T16);    // PC 1 tanspace, 2UV unskinned
  policy.mAvailableVertexFormats.add(lev2::EVTXSTREAMFMT_V12N12T16C4);     // PC 2UV 1 color unskinned
  ////////////////////////////////////////////////////////////////

  toolmesh tmesh;
  tool::DaeReadOpts opts;
  tmesh.readFromAssimp(inf, opts);
  ork::lev2::XgmModel xgmmdlout;
  clusterizeToolMeshToXgmMesh<XgmClusterizerStd>(tmesh, xgmmdlout);
  // assert(false);
  bool rv = ork::lev2::SaveXGM(outf, &xgmmdlout);
  return rv;
}
} // namespace ork::MeshUtil