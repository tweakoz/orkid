///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
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

fmtx4 convertMatrix44(const aiMatrix4x4& inp) {
  fmtx4 rval;
  for (int i = 0; i < 16; i++) {
    int x = i % 4;
    int y = i / 4;
    rval.SetElemXY(x, y, inp[y][x]);
  }
  return rval;
}

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
  float _metallicFactor  = 0.0f;
  float _roughnessFactor = 1.0f;
};

typedef std::map<int, GltfMaterial*> gltfmaterialmap_t;
typedef std::map<std::string, ork::lev2::XgmSkelNode*> skelnodemap_t;

///////////////////////////////////////////////////////////////////////////////

void toolmesh::readFromAssimp(const file::Path& BasePath, tool::DaeReadOpts& readopts) {

  ork::file::Path GlbPath = BasePath;
  auto base_dir           = BasePath.toBFS().parent_path();

  OrkAssert(boost::filesystem::exists(GlbPath.toBFS()));
  OrkAssert(boost::filesystem::is_regular_file(GlbPath.toBFS()));

  printf("base_dir<%s>\n", base_dir.c_str());
  OrkAssert(boost::filesystem::exists(base_dir));
  OrkAssert(boost::filesystem::is_directory(base_dir));

  auto& embtexmap = _varmap.makeValueForKey<lev2::embtexmap_t>("embtexmap");

  printf("BEGIN: importing<%s> via Assimp\n", GlbPath.c_str());
  auto scene = aiImportFile(GlbPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
  printf("END: importing scene<%p>\n", scene);
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

    printf("NumTex<%d>\n", scene->mNumTextures);

    for (int i = 0; i < scene->mNumTextures; i++) {
      auto texture        = scene->mTextures[i];
      std::string fmt     = (const char*)texture->achFormatHint;
      std::string texname = (const char*)texture->mFilename.data;
      bool embedded       = false;

      const void* dataptr = nullptr;
      size_t datalen      = 0;

      auto embtex     = new lev2::EmbeddedTexture;
      embtex->_format = fmt;

      if (texname.length() == 0) {
        embedded            = true;
        embtex->_srcdatalen = texture->mWidth;
        embtex->_srcdata    = (const void*)texture->pcData;
        texname             = FormatString("*%d", i);
        embtex->_name       = texname;
      } else {

        printf("texpath<%s>\n", texname.c_str());
        OrkAssert(false);
      }

      if (fmt == "png" or fmt == "jpg") {
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

    auto find_texture = [&](const std::string texname) -> lev2::EmbeddedTexture* {
      lev2::EmbeddedTexture* rval = nullptr;
      auto it                     = embtexmap.find(texname);
      if (it != embtexmap.end()) {
        rval = it->second;
      } else {
        // find by path
        printf("base_dir<%s> texname<%s>\n", base_dir.c_str(), texname.c_str());
        auto tex_path = base_dir / texname;
        auto tex_ext  = std::string(tex_path.extension().c_str());
        printf("texpath<%s> tex_ext<%s>\n", tex_path.c_str(), tex_ext.c_str());
        if (boost::filesystem::exists(tex_path) and boost::filesystem::is_regular_file(tex_path)) {
          ork::file::Path as_ork_path;
          as_ork_path.fromBFS(tex_path);
          ork::File texfile;
          texfile.OpenFile(as_ork_path, ork::EFM_READ);
          size_t length = 0;
          texfile.GetLength(length);
          printf("texlen<%zu>\n", length);

          if (tex_ext == ".jpg" or tex_ext == ".jpeg" or tex_ext == ".png") {

            auto embtex     = new ork::lev2::EmbeddedTexture;
            embtex->_format = tex_ext.substr(1);

            void* dataptr = malloc(length);
            texfile.Read(dataptr, length);
            texfile.Close();

            embtex->_srcdatalen = length;
            embtex->_srcdata    = (const void*)dataptr;
            std::string texid   = FormatString("*%d", int(embtexmap.size()));
            embtex->_name       = texname;

            embtex->fetchDDSdata();
            embtexmap[texname] = embtex;
            rval               = embtex;
          } else {

            OrkAssert(false);
          }
        } // if (boost::filesystem::exists
      }   // else
      return rval;
    };

    printf("/////////////////////////////////////////////////////////////////\n");

    //////////////////////////////////////////////

    gltfmaterialmap_t materialmap;

    for (int i = 0; i < scene->mNumMaterials; i++) {
      auto material = scene->mMaterials[i];

      auto outmtl    = new GltfMaterial;
      materialmap[i] = outmtl;

      std::string material_name;

      aiColor4D color;
      aiString string;
      float f;
      if (AI_SUCCESS == aiGetMaterialString(material, AI_MATKEY_NAME, &string)) {
        material_name = (const char*)string.data;
        outmtl->_name = material_name;
        printf("has name<%s>\n", material_name.c_str());
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color)) {
        printf("has_uniform_diffuse<%f %f %f %f>\n", color.r, color.g, color.b, color.a);
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &color)) {
        printf("has_uniform_specular<%f %f %f %f>\n", color.r, color.g, color.b, color.a);
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &color)) {
        printf("has_uniform_ambient\n");
      }
      if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &color)) {
        printf("has_uniform_emissive\n");
      }
      if (AI_SUCCESS == aiGetMaterialFloat(material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &f)) {
        outmtl->_metallicFactor = f;
        printf("has_pbr_MetallicFactor<%g>\n", f);
      }
      if (AI_SUCCESS == aiGetMaterialFloat(material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &f)) {
        outmtl->_roughnessFactor = f;
        printf("has_pbr_RoughnessFactor<%g>\n", f);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_colormap = (const char*)string.data;
        auto tex          = find_texture(outmtl->_colormap);
        printf("has_pbr_colormap<%s> tex<%p>\n", outmtl->_colormap.c_str(), tex);
      }
      if (AI_SUCCESS == aiGetMaterialTexture(material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &string)) {
        outmtl->_metallicAndRoughnessMap = (const char*)string.data;
        auto tex                         = find_texture(outmtl->_metallicAndRoughnessMap);
        printf("has_pbr_MetallicAndRoughnessMap<%s> tex<%p>\n", outmtl->_metallicAndRoughnessMap.c_str(), tex);
      }

      if (AI_SUCCESS == material->GetTexture(aiTextureType_NORMALS, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_normalmap = (const char*)string.data;
        auto tex           = find_texture(outmtl->_normalmap);
        printf("has_pbr_normalmap<%s> tex<%p>\n", outmtl->_normalmap.c_str(), tex);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_AMBIENT, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_amboccmap = (const char*)string.data;
        auto tex           = find_texture(outmtl->_amboccmap);
        printf("has_pbr_amboccmap<%s> tex<%p>", outmtl->_amboccmap.c_str(), tex);
      }
      if (AI_SUCCESS == material->GetTexture(aiTextureType_EMISSIVE, 0, &string, NULL, NULL, NULL, NULL, NULL)) {
        outmtl->_emissivemap = (const char*)string.data;
        auto tex             = find_texture(outmtl->_emissivemap);
        printf("has_pbr_emissivemap<%s> tex<%p> \n", outmtl->_emissivemap.c_str(), tex);
      }
      printf("\n");
    }

    printf("/////////////////////////////////////////////////////////////////\n");

    //////////////////////////////////////////////

    std::queue<aiNode*> nodestack;

    skelnodemap_t& xgmskelnodes = _varmap["xgmskelnodes"].Make<skelnodemap_t>();
    std::set<std::string> uniqskelnodeset;

    //////////////////////////////////////////////
    // count, visit dagnodes
    //////////////////////////////////////////////

    bool is_skinned = false;

    nodestack = std::queue<aiNode*>();
    nodestack.push(scene->mRootNode);
    while (not nodestack.empty()) {
      auto n = nodestack.front();
      nodestack.pop();
      auto name = std::string(n->mName.data);
      auto itb  = uniqskelnodeset.find(name);
      if (itb == uniqskelnodeset.end()) {
        int index = uniqskelnodeset.size();
        uniqskelnodeset.insert(name);
        auto xgmnode         = new ork::lev2::XgmSkelNode(name);
        xgmnode->miSkelIndex = index;
        xgmskelnodes[name]   = xgmnode;
        auto matrix          = n->mTransformation;
        printf("uniqNODE<%d:%p> xgmnode<%p> <%s>\n", index, n, xgmnode, name.c_str());
        auto& assimpnodematrix = xgmnode->_varmap["assimpnodematrix"].Make<fmtx4>();
        assimpnodematrix       = convertMatrix44(matrix);
        // assimpnodematrix.dump(name.c_str());
      }
      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }

    printf("/////////////////////////////////////////////////////////////////\n");

    //////////////////////////////////////////////
    // visit meshes, marking dagnodes as bones and fetching joint matrices
    //////////////////////////////////////////////

    struct XgmAssimpVertexWeightItem {
      std::string _bonename;
      float _weight = 0.0f;
    };
    struct XgmAssimpVertexWeights {
      std::vector<XgmAssimpVertexWeightItem> _items;
    };
    std::map<int, XgmAssimpVertexWeights*> assimpweightlut;

    nodestack = std::queue<aiNode*>();
    nodestack.push(scene->mRootNode);
    while (not nodestack.empty()) {
      auto n = nodestack.front();
      nodestack.pop();
      for (int m = 0; m < n->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[n->mMeshes[m]];
        for (int b = 0; b < mesh->mNumBones; b++) {
          auto bone     = mesh->mBones[b];
          auto bonename = std::string(bone->mName.data);
          auto itb      = xgmskelnodes.find(bonename);
          /////////////////////////////////////////////
          // yuk -- assimp is not like gltf, or collada...
          // https://github.com/assimp/assimp/blob/master/code/glTF2/glTF2Importer.cpp#L904
          /////////////////////////////////////////////
          int numvertsaffected = bone->mNumWeights;
          ////////////////////////////////////////////
          if (itb != xgmskelnodes.end()) {
            auto xgmnode = itb->second;
            //////////////////////////////////
            // mark skel node as actual mesh referenced bone
            //////////////////////////////////
            if (false == xgmnode->_varmap["is_bone"].IsA<bool>()) {
              xgmnode->_varmap["is_bone"].Set<bool>(true);
              int index               = xgmnode->miSkelIndex;
              auto matrix             = bone->mOffsetMatrix;
              auto& xgmjointmatrix    = xgmnode->_varmap["assimpoffsetmatrix"].Make<fmtx4>();
              auto& xgmjointmatrixinv = xgmnode->_varmap["assimpoffsetmatrix-inv"].Make<fmtx4>();
              /*printf(
                  "markBONE<%p> xgmnode<%d:%p> <%s> numverts_affected<%d> ",
                  bone,
                  index,
                  xgmnode,
                  bonename.c_str(),
                  numvertsaffected);*/
              for (int j = 0; j < 4; j++) {
                // printf("[");
                for (int k = 0; k < 4; k++) {
                  xgmjointmatrix.SetElemXY(j, k, matrix[j][k]);
                  // printf("%g ", matrix[j][k]);
                }
                // printf("] ");
              }
              xgmjointmatrixinv.inverseOf(xgmjointmatrix);
              // printf("\n");
              /////////////////////////////
              // remember effected verts
              /////////////////////////////
              for (int v = 0; v < numvertsaffected; v++) {
                const aiVertexWeight& vw                   = bone->mWeights[v];
                int vertexID                               = vw.mVertexId;
                float weight                               = vw.mWeight;
                XgmAssimpVertexWeights* weights_for_vertex = nullptr;
                auto itw                                   = assimpweightlut.find(vertexID);
                if (itw != assimpweightlut.end()) {
                  weights_for_vertex = itw->second;
                } else {
                  weights_for_vertex        = new XgmAssimpVertexWeights;
                  assimpweightlut[vertexID] = weights_for_vertex;
                }
                auto xgmw = XgmAssimpVertexWeightItem{bonename, weight};
                weights_for_vertex->_items.push_back(xgmw);
              }
              /////////////////////////////
              is_skinned = true;
            }
          }
        }
      }
      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }

    nodestack = std::queue<aiNode*>();
    nodestack.push(scene->mRootNode);

    //////////////////////////////////////////////

    // std::deque<fmtx4> ork_mtxstack;
    // ork_mtxstack.push_front(convertMatrix44(scene->mRootNode->mTransformation));

    auto it_root_skelnode                 = xgmskelnodes.find(scene->mRootNode->mName.data);
    ork::lev2::XgmSkelNode* root_skelnode = (it_root_skelnode != xgmskelnodes.end()) ? it_root_skelnode->second : nullptr;

    //////////////////////////////////////////////
    // parse nodes
    //////////////////////////////////////////////

    is_skinned = false; // not yet..

    // printf("parsing nodes for meshdata\n");

    while (not nodestack.empty()) {

      auto n = nodestack.front();

      aiMatrix4x4 mtx = n->mTransformation;

      nodestack.pop();

      auto p = n->mParent;
      if (p) {
        auto it_nod_skelnode                 = xgmskelnodes.find(n->mName.data);
        auto it_par_skelnode                 = xgmskelnodes.find(p->mName.data);
        ork::lev2::XgmSkelNode* nod_skelnode = (it_nod_skelnode != xgmskelnodes.end()) ? it_nod_skelnode->second : nullptr;
        ork::lev2::XgmSkelNode* par_skelnode = (it_par_skelnode != xgmskelnodes.end()) ? it_par_skelnode->second : nullptr;
        // printf(
        //  "visit node<%s> parent<%s> xgmskenod<%p> xgmskelpar<%p>\n", n->mName.data, p->mName.data, nod_skelnode, par_skelnode);
        if (par_skelnode) {
          nod_skelnode->mpParent = par_skelnode;
          par_skelnode->mChildren.push_back(nod_skelnode);
        }
      }

      //////////////////////////////////////////////
      // for rigid meshes, preapply transforms
      //  TODO : make optional
      //////////////////////////////////////////////

      fmtx4 ork_model_mtx;
      fmtx3 ork_normal_mtx;
      if (false == is_skinned) {

        printf("/////////////////////////////\n");
        std::deque<aiNode*> nodehier;
        bool done = false;
        auto walk = n;
        while (not done) {
          nodehier.push_back(n);
          fmtx4 test = convertMatrix44(walk->mTransformation);
          test.dump(walk->mName.data);
          walk = walk->mParent;
          done = (walk == nullptr);
        }
        for (auto item : nodehier) {
          ork_model_mtx = convertMatrix44(item->mTransformation) * ork_model_mtx;
        }

        ork_model_mtx = convertMatrix44(n->mTransformation);
        // ork_model_mtx  = ork_model_mtx.dump(n->mName.data);
        ork_normal_mtx = ork_model_mtx.rotMatrix33();
        printf("/////////////////////////////\n");
      }

      //////////////////////////////////////////////
      // visit node
      //////////////////////////////////////////////

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
        MeshUtil::vertex muverts[4];
        // printf("processing numfaces<%d>\n", mesh->mNumFaces);
        for (int t = 0; t < mesh->mNumFaces; ++t) {
          const aiFace* face = &mesh->mFaces[t];
          bool is_triangle   = (face->mNumIndices == 3);

          if (is_triangle) {
            for (int facevert_index = 0; facevert_index < 3; facevert_index++) {
              int index = face->mIndices[facevert_index];
              /////////////////////////////////////////////
              const auto& v  = mesh->mVertices[index];
              const auto& n  = mesh->mNormals[index];
              const auto& uv = (mesh->mTextureCoords[0])[index];
              const auto& b  = (mesh->mBitangents)[index];
              auto& muvtx    = muverts[facevert_index];
              muvtx.mPos     = fvec3(v.x, v.y, v.z).Transform(ork_model_mtx).xyz();
              muvtx.mNrm     = fvec3(n.x, n.y, n.z).Transform(ork_normal_mtx);
              if (has_colors)
                muvtx.mCol[0] = fvec4(1, 1, 1, 1);
              if (has_uvs) {
                muvtx.mUV[0].mMapTexCoord = fvec2(uv.x, uv.y);
                muvtx.mUV[0].mMapBiNormal = fvec3(b.x, b.y, b.z).Transform(ork_normal_mtx);
              }
              /////////////////////////////////////////////
              // yuk -- assimp is not like gltf, or collada...
              // https://github.com/assimp/assimp/blob/master/code/glTF2/glTF2Importer.cpp#L904
              /////////////////////////////////////////////
              if (is_skinned) {
                auto itw = assimpweightlut.find(index);
                if (itw != assimpweightlut.end()) {
                  auto influences = itw->second;
                  int numinf      = influences->_items.size();
                  printf("vertex<%d> raw_numweights<%d>\n", index, numinf);
                  ///////////////////////////////////////////////////
                  // prune to no more than 4 weights
                  ///////////////////////////////////////////////////
                  std::map<float, XgmAssimpVertexWeightItem> largestWeightMap;
                  std::map<float, std::string> prunedWeightMap;
                  std::map<std::string, float> rawweightMap;
                  for (int inf = 0; inf < numinf; inf++) {
                    auto infl                    = influences->_items[inf];
                    float fw                     = infl._weight;
                    rawweightMap[infl._bonename] = fw;
                    if (fw != 0.0f) {
                      largestWeightMap[1.0f - fw] = infl;
                    }
                    // printf( " inf<%d> bone<%s> weight<%g>\n", inf, infl._bonename.c_str(), fw);
                  }
                  int icount      = 0;
                  float totweight = 0.0f;
                  for (auto it : largestWeightMap) {
                    if (icount < 4)
                      totweight += (1.0f - it.first);
                    icount++;
                  }
                  icount             = 0;
                  float newtotweight = 0.0f;
                  for (auto item : largestWeightMap) {
                    if (icount < 4) {
                      // normalize pruned weights
                      float w            = 1.0f - item.first;
                      float fjointweight = w / totweight;
                      newtotweight += fjointweight;
                      std::string name              = item.second._bonename;
                      prunedWeightMap[fjointweight] = name;
                    }
                    ++icount;
                  }
                  float fwtest = fabs(1.0f - newtotweight);
                  if (fwtest >= 0.02f) // ensure within tolerable error limit
                  {
                    printf(
                        "WARNING weight pruning tolerance: <%s> vertex<%d> fwtest<%f> icount<%d> prunedWeightMapSize<%zu>\n",
                        GlbPath.c_str(),
                        index,
                        fwtest,
                        icount,
                        prunedWeightMap.size());
                    // orkerrorlog( "ERROR: <%s> vertex<%d> fwtest<%f> numpairs<%d> largestWeightMap<%d>\n",
                    // policy->mColladaOutName.c_str(), im, fwtest, inumpairs, largestWeightMap.size() ); orkerrorlog( "ERROR:
                    // <%s> cannot prune weight, out of tolerance. You must prune it manually\n", policy->mColladaOutName.c_str()
                    // ); return false;
                  }
                  ///////////////////////////////////////////////////
                  muvtx.miNumWeights = prunedWeightMap.size();
                  assert(muvtx.miNumWeights >= 0);
                  assert(muvtx.miNumWeights <= 4);
                  int windex = 0;
                  /////////////////////////////////
                  // init vertex with no influences
                  /////////////////////////////////
                  for (int iw = 0; iw < 4; iw++) {
                    muvtx.mJointNames[iw]   = root_skelnode->mNodeName;
                    muvtx.mJointWeights[iw] = 0.0f;
                  }
                  /////////////////////////////////
                  for (auto item : prunedWeightMap) {
                    muvtx.mJointNames[windex]   = item.second;
                    muvtx.mJointWeights[windex] = item.first;
                    // printf( "inf<%s:%g> ", item.second.c_str(), item.first );
                    windex++;
                  }
                  // printf( "\n");
                }
              }
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
        // printf("  done processing numfaces<%d> ..\n", mesh->mNumFaces);
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
        auto child     = n->mChildren[i];
        auto child_mtx = child->mTransformation;

        // fmtx4 orkmtx = convertMatrix44(child_mtx);
        // ork_mtxstack.push_back(orkmtx);

        nodestack.push(child);
      }
    }
    // printf("done parsing nodes for meshdata\n");
    printf("/////////////////////////////////////////////////////////////////\n");

    _varmap["is_skinned"].Set<bool>(is_skinned);

    //////////////////////////////////////////////
    // build xgm skeleton
    //////////////////////////////////////////////

    if (is_skinned) {
    }

    //////////////////////////////////////////////
  } // if(scene)

  printf("DONE: readFromAssimp\n");
}

///////////////////////////////////////////////////////////////////////////////

void configureXgmSkeleton(const toolmesh& input, lev2::XgmModel& xgmmdlout) {

  const skelnodemap_t& xgmskelnodes = input._varmap.valueForKey("xgmskelnodes").Get<skelnodemap_t>();

  printf("NumSkelNodes<%d>\n", int(xgmskelnodes.size()));
  xgmmdlout.SetSkinned(true);
  auto& xgmskel = xgmmdlout.RefSkel();
  xgmskel.SetNumJoints(xgmskelnodes.size());
  for (auto& item : xgmskelnodes) {
    const std::string& JointName = item.first;
    auto skelnode                = item.second;
    auto parskelnode             = skelnode->mpParent;
    int idx                      = skelnode->miSkelIndex;
    int pidx                     = parskelnode ? parskelnode->miSkelIndex : -1;
    printf("JointName<%s> skelnode<%p> parskelnode<%p> idx<%d> pidx<%d>\n", JointName.c_str(), skelnode, parskelnode, idx, pidx);

    PoolString JointNameSidx = AddPooledString(JointName.c_str());
    xgmskel.AddJoint(idx, pidx, JointNameSidx);
    xgmskel.RefInverseBindMatrix(idx) = skelnode ? skelnode->mBindMatrixInverse : fmtx4();
    xgmskel.RefJointMatrix(idx)       = parskelnode ? parskelnode->mJointMatrix : fmtx4();
  }
  /////////////////////////////////////
  // flatten the skeleton (WIP)
  /////////////////////////////////////

  auto itroot = xgmskelnodes.find("ROOT");
  if (itroot != xgmskelnodes.end()) {
    auto root = itroot->second;

    xgmskel.miRootNode = root ? root->miSkelIndex : -1;

    if (root) {
      orkstack<lev2::XgmSkelNode*> NodeStack;
      NodeStack.push(root);
      while (false == NodeStack.empty()) {
        lev2::XgmSkelNode* ParNode = NodeStack.top();
        int iparentindex           = ParNode->miSkelIndex;
        NodeStack.pop();
        int inumchildren = ParNode->mChildren.size();
        for (int ic = 0; ic < inumchildren; ic++) {
          lev2::XgmSkelNode* Child = ParNode->mChildren[ic];
          int ichildindex          = Child->miSkelIndex;

          lev2::XgmBone Bone = {iparentindex, ichildindex};

          xgmskel.AddFlatBone(Bone);
          NodeStack.push(Child);
        }
      }
    }
    xgmskel.mpRootNode = root;
    xgmskel.dump();
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

  // printf("BEGIN: clusterizing model\n");
  bool is_skinned = false;
  if (auto as_bool = inp_model._varmap.valueForKey("is_skinned").TryAs<bool>()) {
    is_skinned = as_bool.value();
  }

  auto& inp_embtexmap = inp_model._varmap.typedValueForKey<lev2::embtexmap_t>("embtexmap").value();
  auto& out_embtexmap = out_model._varmap.makeValueForKey<lev2::embtexmap_t>("embtexmap") = inp_embtexmap;

  out_model.ReserveMeshes(inp_model.RefSubMeshLut().size());
  ork::lev2::XgmMesh* out_mesh = new ork::lev2::XgmMesh;
  out_mesh->SetMeshName("Mesh1"_pool);
  out_model.AddMesh("Mesh1"_pool, out_mesh);

  auto VertexFormat = is_skinned ? ork::lev2::EVTXSTREAMFMT_V12N12B12T8I4W4 : ork::lev2::EVTXSTREAMFMT_V12N12B12T16;
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
    // printf("BEGIN: clusterizing submesh<%d>\n", subindex);
    subindex++;
    submesh* inp_submesh = item.second;
    auto& mtlset         = inp_submesh->typedAnnotation<std::set<int>>("materialset");
    auto gltfmtl         = inp_submesh->typedAnnotation<GltfMaterial*>("gltfmaterial");
    assert(mtlset.size() == 1); // assimp does 1 material per submesh

    auto mtlout = new ork::lev2::PBRMaterial();
    mtlout->setTextureBaseName(FormatString("material%d", subindex));
    mtlout->SetName(AddPooledString(gltfmtl->_name.c_str()));
    mtlout->_colorMapName    = gltfmtl->_colormap;
    mtlout->_normalMapName   = gltfmtl->_normalmap;
    mtlout->_amboccMapName   = gltfmtl->_amboccmap;
    mtlout->_mtlRufMapName   = gltfmtl->_metallicAndRoughnessMap;
    mtlout->_metallicFactor  = gltfmtl->_metallicFactor;
    mtlout->_roughnessFactor = gltfmtl->_roughnessFactor;
    out_model.AddMaterial(mtlout);

    auto clusterizer                 = new ClusterizerType;
    ToolMaterialGroup* materialGroup = nullptr;
    auto it                          = mtlmtlmap.find(gltfmtl);
    if (it == mtlmtlmap.end()) {
      materialGroup                  = new ToolMaterialGroup;
      materialGroup->meMaterialClass = ToolMaterialGroup::EMATCLASS_PBR;
      materialGroup->SetClusterizer(clusterizer);
      materialGroup->mMeshConfigurationFlags.mbSkinned = is_skinned;
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
    // printf("END: clusterizing submesh<%d>\n", subindex);
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

      int inumclus               = clusterizer->GetNumClusters();
      xgm_submesh->miNumClusters = inumclus;
      xgm_submesh->mpClusters    = new lev2::XgmCluster[inumclus];
      for (int icluster = 0; icluster < inumclus; icluster++) {
        auto clusterbuilder      = dynamic_cast<XgmClusterBuilder*>(clusterizer->GetCluster(icluster));
        const auto& tool_submesh = clusterbuilder->_submesh;
        clusterbuilder->buildVertexBuffer(VertexFormat);

        lev2::XgmCluster& XgmClus = xgm_submesh->mpClusters[icluster];
        buildTriStripXgmCluster(XgmClus, clusterbuilder);

        // int inumclusjoints = XgmClus.mJoints.size();
        // for( int ib=0; ib<inumclusjoints; ib++ )
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
  options.SetDefault("--dice", "false");
  options.SetDefault("--dicedim", "128.0f");
  options.SetDefault("--in", "yo");
  options.SetDefault("--out", "yo");
  options.SetOptions(toklist);
  const std::string inf  = options.GetOption("--in")->GetValue();
  const std::string outf = options.GetOption("--out")->GetValue();

  bool bDICE = options.GetOption("--dice")->GetValue() == "true";
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
  bool is_skinned = false;
  if (auto as_bool = tmesh._varmap.valueForKey("is_skinned").TryAs<bool>()) {
    is_skinned = as_bool.value();
    if (is_skinned)
      configureXgmSkeleton(tmesh, xgmmdlout);
  }
  policy.mbIsSkinned = is_skinned;
  clusterizeToolMeshToXgmMesh<XgmClusterizerStd>(tmesh, xgmmdlout);
  bool rv = ork::lev2::SaveXGM(outf, &xgmmdlout);
  return rv;
}
} // namespace ork::MeshUtil
