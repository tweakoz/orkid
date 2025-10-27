////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_geoclipmap.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

#include "pyext.h"
#include "pyext_micromesh.inl"
#include "_vdb_impl.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

/////////////////////////////////////////////////

struct SmoothingStage;
using stage_ptr_t = std::shared_ptr<SmoothingStage>;

/////////////////////////////////////////////////

struct SmoothingStage {
  micromesh_ptr_t mesh_inp;
  micromesh_connectivity_ptr_t conn;
  umesh_rprim_ptr_t prim;
  ctx_t context;
  size_t count = 0;
  static void enqueue(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid);
};

/////////////////////////////////////////////////

void SmoothingStage::enqueue(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid) {
  if (inp_stage->count > 0) {
    auto op = [=]() {
      auto mesh_out = inp_stage->mesh_inp->smoothed(inp_stage->conn);
      auto next_stage = std::make_shared<SmoothingStage>();
      next_stage->mesh_inp = mesh_out;
      next_stage->conn = inp_stage->conn;
      next_stage->prim = inp_stage->prim;
      next_stage->context.assign(inp_stage->context);
      next_stage->count = inp_stage->count - 1;
      enqueue(next_stage,colorgrid);
    };
    opq::concurrentQueue()->enqueue(op);
  } else {
    auto op = [=]() {
      if(inp_stage->prim){
        inp_stage->mesh_inp->computeNormals();
        inp_stage->mesh_inp->updateRigidPrim(inp_stage->prim, colorgrid, ctx_t(inp_stage->context.get()));
      }
    };
    opq::mainSerialQueue()->enqueue(op);
  }
}

/////////////////////////////////////////////////

void pyinit_gfx_primitives_rigid(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto micromesh_type = py::class_<MicroMesh, micromesh_ptr_t>(module_lev2, "MicroMesh")
                            //////////////////////////////////////////////////
                            .def_static(
                                "fromVertAndFaceLists",
                                [](py::object vert_data, py::list face_list) -> micromesh_ptr_t {
                                  return std::make_shared<MicroMesh>(vert_data, face_list);
                                },
                                "Create mesh from vertices (list or numpy array) and faces",
                                py::arg("vert_data"), py::arg("face_list"))
                            //////////////////////////////////////////////////
                            .def(
                                "updateFromLists",
                                [](micromesh_ptr_t mesh, py::object vert_data, py::list face_list) {
                                  mesh->updateFromLists(vert_data, face_list);
                                },
                                "Update mesh vertices and faces, reusing existing storage. Vertices can be list or numpy array (N,3) float32",
                                py::arg("vert_data"), py::arg("face_list"))
                            //////////////////////////////////////////////////
                            .def(
                                "updateVertices",
                                [](micromesh_ptr_t mesh, py::object vert_data) {
                                  mesh->updateVertices(vert_data);
                                },
                                "Update only vertices (topology unchanged), reusing existing storage. Accepts list or numpy array (N,3) float32",
                                py::arg("vert_data"))
                            //////////////////////////////////////////////////
                            .def(
                                "updateConnectivity",
                                [](micromesh_ptr_t mesh) {
                                  mesh->updateConnectivity();
                                },
                                "Update internal connectivity data (recomputes vertex adjacency)")
                            //////////////////////////////////////////////////
                            .def(
                                "updateNormals",
                                [](micromesh_ptr_t mesh, py::object normal_data) {
                                  mesh->updateNormals(normal_data);
                                },
                                "Update cached normals from pre-generated data. Accepts list or numpy array (N,3) float32",
                                py::arg("normal_data"))
                            //////////////////////////////////////////////////
                            .def(
                                "computeNormals",
                                [](micromesh_ptr_t mesh) {
                                  mesh->computeNormals();
                                },
                                "Compute and cache normals using internal connectivity")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "vertices",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto verts = py::list();
                                  for (auto& vtx : mesh->_vertices) {
                                    verts.append(fvec3(vtx.x, vtx.y, vtx.z));
                                  }
                                  return verts;
                                })
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "normals",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto normals = py::list();
                                  for (auto& nrm : mesh->_normals) {
                                    normals.append(fvec3(nrm.x, nrm.y, nrm.z));
                                  }
                                  return normals;
                                })
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "tris",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto tris = py::list();
                                  for (auto& face : mesh->_tris) {
                                    tris.append(face);
                                  }
                                  return tris;
                                })
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "quads",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto quads = py::list();
                                  for (auto& face : mesh->_quads) {
                                    quads.append(face);
                                  }
                                  return quads;
                                })
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "num_verts",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_vertices.size();
                                },
                                "Get number of vertices")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "num_faces",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_tris.size() + mesh->_quads.size();
                                },
                                "Get number of faces (tris + quads)")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "num_tris",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_tris.size();
                                },
                                "Get number of triangular faces")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "num_quads",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_quads.size();
                                },
                                "Get number of quad faces")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "faces",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto faces = py::list();
                                  for (auto& q : mesh->_quads) {
                                    faces.append(int(4));
                                    for (auto& idx : q) {
                                      faces.append(idx);
                                    }
                                  }
                                  for (auto& t : mesh->_tris) {
                                    faces.append(int(3));
                                    for (auto& idx : t) {
                                      faces.append(idx);
                                    }
                                  }
                                  return faces;
                                })
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "connectivity",
                                [](micromesh_ptr_t mesh) -> micromesh_connectivity_ptr_t {
                                  return mesh->getConnectivity();
                                },
                                "Get internal connectivity (lazy-evaluated, computed if dirty)")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "vertexConnectivity",
                                [](micromesh_ptr_t mesh) -> micromesh_connectivity_ptr_t {
                                  return mesh->getConnectivity();
                                },
                                "Deprecated: Use 'connectivity' property instead")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "smoothed",
                                [](micromesh_ptr_t mesh, micromesh_connectivity_ptr_t conn) -> micromesh_ptr_t {
                                  py::gil_scoped_release release;
                                  return mesh->smoothed(conn);
                                })
                            //////////////////////////////////////////////////
                            .def(
                                "asyncSmoothed",
                                [](micromesh_ptr_t mesh,              //
                                   micromesh_connectivity_ptr_t conn, //
                                   int num_stages,                    //
                                   py::object prim,
                                   py::object context) { //

                                  auto stage = std::make_shared<SmoothingStage>();
                                  stage->mesh_inp = mesh;
                                  stage->conn = conn;
                                  stage->count = num_stages;
                                  if( context.is_none() ){
                                    stage->context = ctx_t();
                                  }else{
                                    auto as_ctx = py::cast<ctx_t>(context);
                                    stage->context.assign(as_ctx);
                                  }
                                  if( prim.is_none() ){
                                    stage->prim = nullptr;
                                  }else{
                                    OrkAssert( stage->context.get() != nullptr );
                                    stage->prim = prim.cast<umesh_rprim_ptr_t>();
                                  }
                                  SmoothingStage::enqueue(stage,nullptr);
                                })
                            //////////////////////////////////////////////////
                            .def(
                                "asyncSmoothedWithColorGrid",
                                [](micromesh_ptr_t mesh,              //
                                   micromesh_connectivity_ptr_t conn, //
                                   vdb_vec3grid_ptr_t colorgrid,      //
                                   int num_stages,                    //
                                   umesh_rprim_ptr_t prim,
                                   ctx_t context) { //

                                  auto stage = std::make_shared<SmoothingStage>();
                                  stage->mesh_inp = mesh;
                                  stage->conn = conn;
                                  stage->prim = prim;
                                  stage->context.assign(context);
                                  stage->count = num_stages;
                                  SmoothingStage::enqueue(stage,colorgrid);
                                });
  /////////////////////////////////////////////////////////////////////////////////
  auto micromesh_conn_type = py::class_<MicroMeshConnectivity, micromesh_connectivity_ptr_t>(module_lev2, "MicroMeshConnectivity");
  /////////////////////////////////////////////////////////////////////////////////
  auto rprimbase_t =
      py::class_<meshutil::RigidPrimitiveBase, meshutil::rigidprimitive_ptr_t>(module_lev2, "meshutil::RigidPrimitiveBase")
          .def(
              "createNode",
              [](meshutil::rigidprimitive_ptr_t prim,                            //
                 std::string named,                                              //
                 scenegraph::layer_ptr_t layer,                                  //
                 fxpipeline_ptr_t pipeline) -> scenegraph::drawable_node_ptr_t { //
                auto node                                                        //
                    = prim->createNode(named, layer, pipeline);
                // node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
                return node;
              })
          .def(
              "createNode",
              [](meshutil::rigidprimitive_ptr_t prim,                           //
                 std::string named,                                             //
                 scenegraph::layer_ptr_t layer,                                 //
                 lev2::material_ptr_t mtl) -> scenegraph::drawable_node_ptr_t { //
                auto node                                                       //
                    = prim->createNode(named, layer, mtl);
                // node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
                return node;
              })
          .def(
              "createDrawable",
              [](meshutil::rigidprimitive_ptr_t prim,                          //
                 fxpipeline_ptr_t pipeline) -> lev2::callback_drawable_ptr_t { //
                auto drw = prim->createDrawable(pipeline);
                return drw;
              })
          .def(
              "createDrawable",
              [](meshutil::rigidprimitive_ptr_t prim,                   //
                 material_ptr_t mtl) -> lev2::callback_drawable_ptr_t { //
                auto drw = prim->createDrawable(mtl);
                return drw;
              })
          .def(
              "createDrawableData",
              [](meshutil::rigidprimitive_ptr_t prim,                                    //
                 fxpipeline_ptr_t pipeline) -> meshutil::rigidprimitive_drawdata_ptr_t { //
                auto drwdata        = std::make_shared<meshutil::RigidPrimitiveDrawableData>();
                drwdata->_pipeline  = pipeline;
                drwdata->_primitive = prim;
                return drwdata;
              })
              .def_property(
                  "debugState",
                  [](meshutil::rigidprimitive_ptr_t prim) -> bool {
                    return prim->_stateDebugger;
                  },
                  [](meshutil::rigidprimitive_ptr_t prim, bool value) {
                    prim->_stateDebugger = value;
                  });
  type_codec->registerStdCodec<meshutil::rigidprimitive_ptr_t>(rprimbase_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<meshutil::rigidprim_V12N12B12T8C4_t, meshutil::RigidPrimitiveBase, meshutil::rigidprim_V12N12B12T8C4_ptr_t>(
      module_lev2, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](meshutil::submesh_ptr_t submesh, ctx_t context) {
        auto prim = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t>();
        prim->fromSubMesh(*submesh, context.get());
        return prim;
      }))
      .def(
          "fromSubMesh",                                   //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             meshutil::submesh_ptr_t submesh,              //
             ctx_t context) {                              //
            prim->fromSubMesh(*submesh, context.get());
          })
      .def(
          "updateWithMicroMesh",                                 //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             micromesh_ptr_t micromesh,                    //
             ctx_t context) {                              //
             micromesh->updateRigidPrim(prim, nullptr, context);
          })
      .def(
          "fromVertsAndFacesDict",                         //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             py::object verts,                             //
             py::list faces,                               //
             ctx_t context) {                              //
            ////////////////////////////////////////////
            auto micromesh = std::make_shared<MicroMesh>(verts, faces);
            micromesh->updateRigidPrim(prim, nullptr, context);
          },
          "Create rigid primitive from vertices (list or numpy array (N,3) float32) and faces",
          py::arg("verts"), py::arg("faces"), py::arg("context"))
      .def("renderEML", [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, ctx_t context) { //
        prim->renderEML(context.get());
      });
} // void pyinit_gfx_rigidprim(py::module& module_lev2) {
} // namespace ork::lev2
