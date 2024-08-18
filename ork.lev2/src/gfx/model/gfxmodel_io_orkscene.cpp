#include <boost/filesystem.hpp>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include "../meshutil/assimp_util.inl"
#include <ork/util/logger.h>
using namespace std::literals;
namespace bfs = boost::filesystem;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_mioROSCN = logger()->createChannel("orksceneREAD", fvec3(0.8, 0.8, 0.4), true);

bool XgmModel::_loadOrkScene(XgmModel* mdl, datablock_ptr_t datablock) {
  ork::lev2::XgmModel xgmmdlout;
  auto base_dir = datablock->_vars->typedValueForKey<bfs::path>("base-directory").value();
  // check for json "orkscene" object
  auto as_cstr = (const char*)datablock->data();
  logchan_mioROSCN->log("as_cstr: %s", as_cstr);
  size_t datalen = datablock->length();
  rapidjson::Document _document;
  _document.Parse(as_cstr);
  bool is_object = _document.IsObject();
  bool has_top   = _document.HasMember("orkscene");
  OrkAssert(is_object);
  OrkAssert(has_top);
  const auto& topnode = _document["orkscene"];

  for (auto it = topnode.MemberBegin(); //
       it != topnode.MemberEnd();
       ++it) {

    const auto& propkey = it->name;
    const auto& sgnode  = it->value;
    auto nodename       = propkey.GetString();
    logchan_mioROSCN->log("nodename<%s>", nodename);
    OrkAssert(sgnode.GetType() == rapidjson::kObjectType);

    auto top_mesh   = std::make_shared<meshutil::Mesh>();
    auto& embtexmap = top_mesh->_varmap->makeValueForKey<lev2::embtexmap_t>("embtexmap");
    meshutil::gltfmaterialmap_t materialmap;

    top_mesh->_vertexExtents.BeginGrow();

    /////////////////////////////////
    // fetch texture lambda
    /////////////////////////////////

    auto parse_texture =
        [&](const std::string& texture_name, const std::string& channel_name, ETextureUsage usage) -> embtex_ptr_t {
      embtex_ptr_t rval = nullptr;
      bfs::path tex_path;
      if (texture_name.find("://") == std::string::npos) {
        tex_path = base_dir / texture_name;
      } else {
        auto as_ork = file::Path(texture_name);
        tex_path    = as_ork.toAbsolute().c_str();
      }
      auto tex_ext = std::string(tex_path.extension().c_str());
      logchan_mioROSCN->log("checking tex<%s>", tex_path.c_str());
      if (boost::filesystem::exists(tex_path) and boost::filesystem::is_regular_file(tex_path)) {
        ork::file::Path as_ork_path;
        as_ork_path.fromBFS(tex_path);
        ork::File texfile;
        texfile.OpenFile(as_ork_path, ork::EFM_READ);
        size_t length = 0;
        texfile.GetLength(length);
        // logchan_meshutilassimp->log("texlen<%zu>", length);

        if (tex_ext == ".jpg" or tex_ext == ".jpeg" or tex_ext == ".png" or tex_ext == ".tga" or tex_ext == ".dds") {

          rval          = std::make_shared<EmbeddedTexture>();
          rval->_format = tex_ext.substr(1);
          rval->_usage  = usage;

          void* dataptr = malloc(length);
          texfile.Read(dataptr, length);
          texfile.Close();

          rval->_srcdatalen = length;
          rval->_srcdata    = (const void*)dataptr;
          std::string texid = FormatString("*%d", int(embtexmap.size()));
          rval->_name       = texture_name;

          rval->fetchDDSdata();
          logchan_mioROSCN->log("added tex<%s>", tex_path.c_str());
          embtexmap[texture_name.c_str()] = rval;
        }
      }
      return rval;
    };

    /////////////////////////////////
    // fetch material
    /////////////////////////////////

    int material_index = 0;
    std::map<std::string, meshutil::GltfMaterial*> materials_by_name;
    if( sgnode.HasMember("materials") ){

      const auto& materials = sgnode["materials"];

      for (auto it_mtls = materials.MemberBegin(); //
           it_mtls != materials.MemberEnd();
           ++it_mtls) {

        const auto& mtl_name = it_mtls->name;
        const auto& material = it_mtls->value;

        auto material_type   = material["type"].GetString();
        logchan_mioROSCN->log("material_type<%s>", material_type);
        auto outmtl    = new meshutil::GltfMaterial;
          outmtl->_name            = mtl_name.GetString();

        auto& out_group = top_mesh->MergeSubMesh(mtl_name.GetString());
        out_group.typedAnnotation<meshutil::GltfMaterial*>("gltfmaterial") = outmtl;

        materialmap[material_index++] = outmtl;
        materials_by_name[mtl_name.GetString()] = outmtl;
        if ("emissive"s == material_type) {
          outmtl->_baseColor       = fvec4(1, 1, 1, 1);
          outmtl->_metallicFactor  = 0;
          outmtl->_roughnessFactor = 1;
          auto emissivemap         = material["emissivemap"].GetString();
          outmtl->_emissivemap     = emissivemap;
          auto embtex              = parse_texture(emissivemap, "emissivemap", ETEXUSAGE_COLOR);
        } else if ("pbr"s == material_type) {
          outmtl->_baseColor       = fvec4(1, 1, 1, 1);
          outmtl->_metallicFactor  = 0;
          outmtl->_roughnessFactor = 1;
          if (material.HasMember("baseColor")) {
            const auto& bc       = material["baseColor"].GetArray();
            outmtl->_baseColor.x = bc[0].GetDouble();
            outmtl->_baseColor.y = bc[1].GetDouble();
            outmtl->_baseColor.z = bc[2].GetDouble();
          }
          if (material.HasMember("metallicFactor")) {
            outmtl->_metallicFactor = material["metallicFactor"].GetDouble();
          }
          if (material.HasMember("roughnessFactor")) {
            outmtl->_roughnessFactor = material["roughnessFactor"].GetDouble();
          }
          if (material.HasMember("colorMap")) {
            auto texture_name = material["colorMap"].GetString();
            ;
            outmtl->_colormap = texture_name;
            auto embtex       = parse_texture(texture_name, "colormap", ETEXUSAGE_COLOR);
          }
          if (material.HasMember("mrMap")) {
            auto texture_name = material["mrMap"].GetString();
            ;
            outmtl->_metallicAndRoughnessMap = texture_name;
            auto embtex                      = parse_texture(texture_name, "colormap", ETEXUSAGE_COLOR);
          }
          if (material.HasMember("normalMap")) {
            auto texture_name = material["normalMap"].GetString();
            ;
            outmtl->_normalmap = texture_name;
            auto embtex        = parse_texture(texture_name, "colormap", ETEXUSAGE_NORMAL);
          }
          if (material.HasMember("aoMap")) {
            auto texture_name = material["aoMap"].GetString();
            ;
            outmtl->_amboccmap = texture_name;
            auto embtex        = parse_texture(texture_name, "colormap", ETEXUSAGE_GREYSCALE);
          }
          if (material.HasMember("emissivemap")) {
            auto texture_name = material["emissivemap"].GetString();
            ;
            outmtl->_emissivemap = texture_name;
            auto embtex          = parse_texture(texture_name, "colormap", ETEXUSAGE_GREYSCALE);
          }
        }
      }
    }


    /////////////////////////////////
    // fetch meshes for material
    /////////////////////////////////

    const auto& meshes = sgnode["meshes"];
    for (auto itm = meshes.MemberBegin(); //
         itm != meshes.MemberEnd();
         ++itm) {
      auto meshname        = itm->name.GetString();
      const auto& meshsubo = itm->value;

      fvec3 load_trans;
      fquat load_orient;

      auto meshfile = meshsubo["file"].GetString();

      std::string mesh_mtl = meshsubo["material"].GetString();
      auto it_m = materials_by_name.find(mesh_mtl);
      OrkAssert(it_m != materials_by_name.end());
      auto outmtl = it_m->second;

      auto& out_group = top_mesh->MergeSubMesh(mesh_mtl.c_str());

      if (meshsubo.HasMember("translation")) {
        const auto& ary = meshsubo["translation"].GetArray();
        load_trans.x    = ary[0].GetDouble();
        load_trans.y    = ary[1].GetDouble();
        load_trans.z    = ary[2].GetDouble();
      }
      if (meshsubo.HasMember("orientation")) {
        const auto& ary = meshsubo["orientation"].GetArray();
        load_orient.w   = ary[0].GetDouble();
        load_orient.x   = ary[1].GetDouble();
        load_orient.y   = ary[2].GetDouble();
        load_orient.z   = ary[3].GetDouble();
      }
      if (meshsubo.HasMember("axisAngle")) {
        const auto& ary = meshsubo["axisAngle"].GetArray();
        fvec3 axis;
        float angle;
        axis.x      = ary[0].GetDouble();
        axis.y      = ary[1].GetDouble();
        axis.z      = ary[2].GetDouble();
        angle       = ary[3].GetDouble();
        load_orient = fquat(axis, angle);
      }

      auto mesh_path = base_dir / meshfile;
      logchan_mioROSCN->log("meshname<%s>", meshname);
      logchan_mioROSCN->log("mesh_path<%s>", mesh_path.c_str());
      auto mesh = std::make_shared<meshutil::Mesh>();
      mesh->_loadXF.fromQuaternion(load_orient);
      mesh->_loadXF.setTranslation(load_trans);
      
     if( false ) { //mesh_path.extension() == ".obj" ){
        mesh->ReadFromWavefrontObj(mesh_path.c_str());
     }
     else{
        mesh->readFromAssimp(mesh_path.c_str());
     }

    for (auto pgitem : mesh->_submeshesByPolyGroup) {
        auto pgname  = pgitem.first;
        auto pgroup  = pgitem.second;
        int numpolys = pgroup->numPolys(0);

        auto min = pgroup->boundingMin();
        auto max = pgroup->boundingMax();

        meshutil::submesh as_tris;
        meshutil::submeshTriangulate(*pgroup, as_tris);

        OrkAssert(outmtl != nullptr);

        if (meshsubo.HasMember("recomputeBasis")) {
          const auto& do_basis = meshsubo["recomputeBasis"].GetBool();
          meshutil::submesh smoothed;
          meshutil::submeshWithFaceNormalsAndBinormals(as_tris, smoothed);
          out_group.MergeSubMesh(smoothed);
        } else {
          out_group.MergeSubMesh(as_tris);
        }

        top_mesh->_vertexExtents.Grow(dvec3_to_fvec3(min));
        top_mesh->_vertexExtents.Grow(dvec3_to_fvec3(max));

        logchan_mioROSCN->log("  submesh<%s> numpolys<%d>", pgname.c_str(), numpolys);
      }
    }

    top_mesh->_vertexExtents.EndGrow();

    top_mesh->dumpStats();


    meshutil::clusterizeToolMeshToXgmMesh<meshutil::XgmClusterizerStd>(*top_mesh, xgmmdlout);

    auto vmin          = top_mesh->_vertexExtents.Min();
    auto vmax          = top_mesh->_vertexExtents.Max();
    auto center        = (vmax + vmin) * 0.5f;
    auto abs_vmaxdelta = (vmax - center).absolute();
    auto abs_vmindelta = (vmin - center).absolute();
    auto abs_max       = abs_vmaxdelta.maxXYZ(abs_vmindelta);
    /////////////////////////
    // compute radius
    /////////////////////////

    float radius   = 0.0f;
    float vmax_mag = (vmax - center).magnitude();
    float vmin_mag = (vmin - center).magnitude();
    if (vmax_mag > radius)
      radius = vmax_mag;
    if (vmin_mag > radius)
      radius = vmin_mag;
    /////////////////////////

    xgmmdlout.mBoundingCenter = center;
    xgmmdlout.mBoundingRadius = radius;
    xgmmdlout.mAABoundXYZ     = center;
    xgmmdlout.mAABoundWHD     = abs_max;

    printf( "center<%g %g %g>\n", center.x, center.y, center.z );
    printf( "radius<%g>\n", radius );
  }
  auto xgm_dblock = writeXgmToDatablock(&xgmmdlout);
  return _loadXGM(mdl, xgm_dblock);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
