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

    /////////////////////////////////
    // fetch texture lambda
    /////////////////////////////////

    auto parse_texture =
        [&](const std::string& texture_name, const std::string& channel_name, ETextureUsage usage) -> EmbeddedTexture* {
      EmbeddedTexture* rval = nullptr;
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

        if (tex_ext == ".jpg" or tex_ext == ".jpeg" or tex_ext == ".png" or tex_ext == ".tga") {

          rval          = new EmbeddedTexture;
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

    const auto& material = sgnode["material"];
    auto material_type   = material["type"].GetString();
    logchan_mioROSCN->log("material_type<%s>", material_type);
    auto outmtl    = new meshutil::GltfMaterial;
    materialmap[0] = outmtl;
    if ("emissive"s == material_type) {
      auto texture_name        = material["texture"].GetString();
      outmtl->_name            = texture_name;
      outmtl->_baseColor       = fvec4(1, 1, 1, 1);
      outmtl->_metallicFactor  = 0;
      outmtl->_roughnessFactor = 1;
      outmtl->_colormap        = texture_name;
      logchan_mioROSCN->log("texture_name<%s>", texture_name);
      auto embtex = parse_texture(texture_name, "colormap", ETEXUSAGE_COLOR);
    }

    /////////////////////////////////
    // fetch meshes for material
    /////////////////////////////////

    const auto& meshes = sgnode["meshes"];
    for (auto itm = meshes.MemberBegin(); //
         itm != meshes.MemberEnd();
         ++itm) {
      auto meshname  = itm->name.GetString();
      auto meshfile  = itm->value.GetString();
      auto mesh_path = base_dir / meshfile;
      logchan_mioROSCN->log("meshname<%s>", meshname);
      logchan_mioROSCN->log("mesh_path<%s>", mesh_path.c_str());
      auto mesh = std::make_shared<meshutil::Mesh>();
      mesh->ReadFromWavefrontObj(mesh_path.c_str());
      for (auto pgitem : mesh->_submeshesByPolyGroup) {
        auto pgname  = pgitem.first;
        auto pgroup  = pgitem.second;
        int numpolys = pgroup->numPolys(0);
        top_mesh->MergeSubMesh(*pgroup);
        logchan_mioROSCN->log("  submesh<%s> numpolys<%d>", pgname.c_str(), numpolys);
      }
    }
    top_mesh->dumpStats();
    auto& out_submesh = top_mesh->MergeSubMesh("default");
    auto& mtlset      = out_submesh.typedAnnotation<std::set<int>>("materialset");
    mtlset.insert(0);
    out_submesh.typedAnnotation<meshutil::GltfMaterial*>("gltfmaterial") = outmtl;

    meshutil::clusterizeToolMeshToXgmMesh<meshutil::XgmClusterizerStd>(*top_mesh, *mdl);

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

    mdl->mBoundingCenter = center;
    mdl->mBoundingRadius = radius;
    mdl->mAABoundXYZ     = center;
    mdl->mAABoundWHD     = abs_max;
  }
  auto xgm_dblock = writeXgmToDatablock(mdl);
  return _loadXGM(mdl, xgm_dblock);
}


///////////////////////////////////////////////////////////////////////////////
} //namespace ork { namespace lev2 {
