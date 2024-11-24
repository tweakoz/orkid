////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/primitives_points.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/util/NullInterrupter.h>
#include <random>
#include <vector>
#include <iostream>
#include <cmath>


///////////////////////////////////////////////////////////////////////////////

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

using floatgrid_t       = openvdb::FloatGrid;
using floatgrid_ptr_t   = std::shared_ptr<floatgrid_t>;
using points_prim_t     = primitives::PointsPrimitive<VtxV12C4>;
using points_prim_ptr_t = std::shared_ptr<points_prim_t>;

///////////////////////////////////////////////////////////////////////////////

std::string shadertext = R"(
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
  float pointsize;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx_points : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 col : COLOR0;
  }
  outputs {
    vec3 frg_col;
  }
}
////////////////////////////////////////
fragment_interface iface_frg_points : ublock_frg {
  inputs {
    vec3 frg_col;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_points : iface_vtx_points {
  frg_col = col.xyz;
  gl_Position = mvp * vec4(pos.x,pos.y,pos.z,1);
  gl_PointSize = 3.0;
}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {
  out_clr = vec4(frg_col.xyz,1);
}

////////////////////////////////////////
technique tek_points_fwd {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_points;
    fragment_shader = ps_points;
    state_block     = default;
  }
}
)"; // R"()" syntax for raw string literals

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Resources {

  Resources(Context* ctx, orkezapp_ptr_t ezapp, floatgrid_ptr_t grid) {

    _uicamera                 = std::make_shared<EzUiCam>();
    _uicamera->_constrainZ    = true;
    _uicamera->_base_zmoveamt = 2.0f;
    _uicamera->mfLoc          = 25.0f;
    _uicamera->_loc_min       = 1.0f;
    _uicamera->_loc_max      = 200.0f;

    _camlut                = std::make_shared<CameraDataLut>();
    (*_camlut)["spawncam"] = _uicamera->_camcamdata;

    ///////////////////////////////////////////////////
    // init material
    ///////////////////////////////////////////////////

    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInitFromShaderText(ctx, "yo", shadertext);
    auto fxtechnique    = _material->technique("tek_points_fwd");
    auto fxparameterMVP = _material->param("mvp");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    _material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);

    ///////////////////////////////////////////////////
    // create RCFD, RCID
    ///////////////////////////////////////////////////

    auto sg_params                                    = std::make_shared<varmap::VarMap>();
    sg_params->makeValueForKey<std::string>("preset") = "ForwardPBR";
    _scenegraph = std::make_shared<scenegraph::Scene>();
    _scenegraph->initWithParams(sg_params);
    auto lyr_std = _scenegraph->createLayer("std_forward");
    auto lyr_dpp = _scenegraph->createLayer("depth_prepass");

    ///////////////////////////////////////////////////
    // create grid node
    ///////////////////////////////////////////////////

    _griddata = std::make_shared<GridDrawableData>();
    _griddata->_shader_suffix = "_V2";
    _grid_drawable = _griddata->createDrawable();
    _node_grid = lyr_std->createDrawableNode("grid", _grid_drawable);

    ///////////////////////////////////////////////////
    auto fxcache = _material->pipelineCache();
    FxPipelinePermutation permu;
    permu._rendering_model = "FORWARD_PBR"_crcu;
    permu._forced_technique = fxtechnique;
    _pipeline                          = fxcache->findPipeline(permu);
    _pipeline->_params[fxparameterMVP] = "RCFD_Camera_MVP_Mono"_crcsh;

    ///////////////////////////////////////////////////
    // init points primitive
    ///////////////////////////////////////////////////

    int num_points   = grid->tree().activeLeafVoxelCount();
    _points_prim     = std::make_shared<points_prim_t>(num_points);
    VtxV12C4* points = _points_prim->lock(ctx);

    int point_index = 0;
    for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
      const auto& leaf = *leafIter;

      // Iterate over active voxels within the leaf
      for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
        // Get the voxel coordinates and value
        openvdb::Coord coord = voxelIter.getCoord();
        float value          = *voxelIter;

        // Convert voxel coordinates to world coordinates
        openvdb::Vec3f worldPosition = grid->transform().indexToWorld(coord);

        // Populate the points array (adapt as necessary)
        points[point_index].x = worldPosition.x();
        points[point_index].y = worldPosition.y();
        points[point_index].z = worldPosition.z();
        uint32_t bgra         = 0;
        bgra |= (uint32_t(value * 255.0f) & 0xff) << 16;
        bgra |= (uint32_t(value * 255.0f) & 0xff) << 8;
        bgra |= (uint32_t(value * 255.0f) & 0xff) << 0;

        points[point_index].color = bgra;

        point_index++;
      }
    }
    _points_prim->unlock(ctx);

    ezapp->_mainWindow->_execsceneparams = _scenegraph->_params;
    ezapp->_mainWindow->_execscene       = _scenegraph;
    _node = _points_prim->createNode("points", lyr_std, _pipeline);

  }

  cameradatalut_ptr_t _camlut;
  freestyle_mtl_ptr_t _material;
  points_prim_ptr_t _points_prim;
  fxpipeline_ptr_t _pipeline;
  scenegraph::scene_ptr_t _scenegraph;
  scenegraph::node_ptr_t _node;
  scenegraph::node_ptr_t _node_grid;
  griddrawabledataptr_t _griddata;
  drawable_ptr_t _grid_drawable;
  lev2::ezuicam_ptr_t _uicamera;
};

using resources_ptr_t = std::shared_ptr<Resources>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  auto desc = init_data->commandLineOptions("minimal3d example Options");
  desc->add_options()                  //
      ("help", "produce help message") //
      ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*0,1,2,3,4)") //
      ("vdb", po::value<std::string>(), "vdb file to load");

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }

  init_data->_ssaa_samples = vars["ssaa"].as<int>();

  ///////////////////////////////////////////////////
  // load VDB file
  ///////////////////////////////////////////////////

  auto vdbfile = vars["vdb"].as<std::string>();
  openvdb::initialize();
  openvdb::io::File file(vdbfile);
  file.open();
  openvdb::GridBase::Ptr baseGrid;
  for (openvdb::io::File::NameIterator nameIter = file.beginName(); nameIter != file.endName(); ++nameIter) {
    baseGrid = file.readGrid(nameIter.gridName());
  }
  file.close();
  floatgrid_ptr_t grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

  ///////////////////////////////////////////////////

  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;
  resources_ptr_t resources;
  //////////////////////////////////////////////////////////
  Timer timer;
  timer.Start();
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { //
    resources = std::make_shared<Resources>(ctx, ezapp, grid);
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) { resources = nullptr; });
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    auto context = drwev->GetTarget();
    ork::opq::mainSerialQueue()->Process();
    resources->_scenegraph->renderOnContext(context);
  });
  //////////////////////////////////////////////////////////
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) { //
    resources->_scenegraph->enqueueToRenderer(resources->_camlut); //
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { //
    resources->_scenegraph->_compositorImpl->compositingContext().Resize(w, h); //
  });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult { //
    bool handled = resources->_uicamera->UIEventHandler(ev);
    return ui::HandlerResult();
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
