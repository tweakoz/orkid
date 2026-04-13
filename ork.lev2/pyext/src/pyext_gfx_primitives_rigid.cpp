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
  bool _validate = false;
  static void enqueue(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid);
  static micromesh_ptr_t synchronous(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid);
};

/////////////////////////////////////////////////

void SmoothingStage::enqueue(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid) {
  if (inp_stage->count > 0) {
    auto op = [=]() {
      auto mesh_out = inp_stage->mesh_inp->smoothed(inp_stage->conn);
      if (inp_stage->_validate) {
        printf("SmoothingStage: Validating mesh after stage (remaining=%zu)\n", inp_stage->count - 1);
        mesh_out->validate();
      }
      auto next_stage = std::make_shared<SmoothingStage>();
      next_stage->mesh_inp = mesh_out;
      // Clone connectivity for thread safety - each stage gets its own copy
      next_stage->conn = std::make_shared<MicroMeshConnectivity>(*inp_stage->conn);
      next_stage->prim = inp_stage->prim;
      next_stage->context.assign(inp_stage->context);
      next_stage->count = inp_stage->count - 1;
      next_stage->_validate = inp_stage->_validate;
      enqueue(next_stage,colorgrid);
    };

    // Enqueue operation to mesh's async queue
    inp_stage->mesh_inp->_async_operations.atomicOp([op](void_lambda_list_t& ops) {
      ops.push_back(op);
    });

  } else {
    auto op = [=]() {
      if(inp_stage->prim){
        if (inp_stage->_validate) {
          printf("SmoothingStage: Validating mesh before computeNormals (final stage)\n");
          inp_stage->mesh_inp->validate();
        }
        inp_stage->mesh_inp->computeNormals();
        inp_stage->mesh_inp->updateRigidPrim(inp_stage->prim, colorgrid, ctx_t(inp_stage->context.get()));
      }
    };

    // Enqueue final operation to mesh's async queue
    inp_stage->mesh_inp->_async_operations.atomicOp([op](void_lambda_list_t& ops) {
      ops.push_back(op);
    });
  }
}

micromesh_ptr_t SmoothingStage::synchronous(stage_ptr_t inp_stage,vdb_vec3grid_ptr_t colorgrid) {
  micromesh_ptr_t current_mesh = inp_stage->mesh_inp;
  for (size_t i = 0; i < inp_stage->count; i++) {
    current_mesh = current_mesh->smoothed(inp_stage->conn);
  }
  current_mesh->computeNormals();
  return current_mesh;
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
                                "updateColors",
                                [](micromesh_ptr_t mesh, py::object color_data) {
                                  mesh->updateColors(color_data);
                                },
                                "Update vertex colors. Accepts list of vec3 or numpy array (N,3) float32",
                                py::arg("color_data"))
                            //////////////////////////////////////////////////
                            .def(
                                "updateUVs",
                                [](micromesh_ptr_t mesh, py::object uv_data) {
                                  mesh->updateUVs(uv_data);
                                },
                                "Update UV coordinates. Accepts list of vec2 or numpy array (N,2) float32",
                                py::arg("uv_data"))
                            //////////////////////////////////////////////////
                            .def(
                                "updateBinormals",
                                [](micromesh_ptr_t mesh, py::object binormal_data) {
                                  mesh->updateBinormals(binormal_data);
                                },
                                "Update binormals from pre-generated data. Accepts list or numpy array (N,3) float32",
                                py::arg("binormal_data"))
                            //////////////////////////////////////////////////
                            .def(
                                "computeNormals",
                                [](micromesh_ptr_t mesh) {
                                  mesh->computeNormals();
                                },
                                "Compute and cache normals using internal connectivity")
                            //////////////////////////////////////////////////
                            .def(
                                "computeBinormals",
                                [](micromesh_ptr_t mesh) {
                                  mesh->computeBinormals();
                                },
                                "Compute binormals from normals using up vector as reference")
                            //////////////////////////////////////////////////
                            .def(
                                "validate",
                                [](micromesh_ptr_t mesh) -> bool {
                                  return mesh->validate();
                                },
                                "Validate mesh integrity. Returns True if valid, False otherwise. Logs detailed diagnostics.")
                            //////////////////////////////////////////////////
                            .def(
                                "smooth",
                                [](micromesh_ptr_t mesh, 
                                   micromesh_connectivity_ptr_t conn) -> micromesh_ptr_t {
                                  py::gil_scoped_release release;
                                  return mesh->smoothed(conn);
                                })
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
                                "uvs",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto uvs = py::list();
                                  for (auto& uv : mesh->_uvs) {
                                    uvs.append(fvec2(uv.x, uv.y));
                                  }
                                  return uvs;
                                },
                                "Get UV coordinates")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "binormals",
                                [](micromesh_ptr_t mesh) -> py::list {
                                  auto binormals = py::list();
                                  for (auto& bn : mesh->_binormals) {
                                    binormals.append(fvec3(bn.x, bn.y, bn.z));
                                  }
                                  return binormals;
                                },
                                "Get binormals")
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
                                "num_uvs",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_uvs.size();
                                },
                                "Get number of UV coordinates")
                            //////////////////////////////////////////////////
                            .def_property_readonly(
                                "num_binormals",
                                [](micromesh_ptr_t mesh) -> size_t {
                                  return mesh->_binormals.size();
                                },
                                "Get number of binormals")
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
                                  micromesh_connectivity_ptr_t conn;
                                  {
                                    py::gil_scoped_release release;
                                    conn = mesh->getConnectivity();
                                  }
                                  return conn;
                                },
                                "Deprecated: Use 'connectivity' property instead")
                            //////////////////////////////////////////////////
                            .def(
                                "asyncSmoothed",
                                [](micromesh_ptr_t mesh,              //
                                   micromesh_connectivity_ptr_t conn, //
                                   int num_stages,                    //
                                   py::object prim,
                                   py::object context,
                                   bool validate = false) { //

                                  auto stage = std::make_shared<SmoothingStage>();
                                  stage->mesh_inp = mesh;
                                  // Clone connectivity at entry point so each smoothing chain is isolated
                                  stage->conn = std::make_shared<MicroMeshConnectivity>(*conn);
                                  stage->count = num_stages;
                                  stage->_validate = validate;
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
                                },
                                py::arg("conn"),
                                py::arg("num_stages"),
                                py::arg("prim")=py::none(),
                                py::arg("context")=py::none(),
                                py::arg("validate") = false)
                            //////////////////////////////////////////////////
                            .def(
                                "asyncSmoothedWithColorGrid",
                                [](micromesh_ptr_t mesh,              //
                                   micromesh_connectivity_ptr_t conn, //
                                   vdb_vec3grid_ptr_t colorgrid,      //
                                   int num_stages,                    //
                                   umesh_rprim_ptr_t prim,
                                   ctx_t context,
                                   bool validate = false) { //
                                    auto op = [=]() {
                                      auto stage = std::make_shared<SmoothingStage>();
                                      stage->mesh_inp = mesh;
                                      // Clone connectivity at entry point so each smoothing chain is isolated
                                      stage->conn = std::make_shared<MicroMeshConnectivity>(*conn);
                                      //stage->prim = prim;
                                      stage->context.assign(context);
                                      stage->count = num_stages;
                                      stage->_validate = validate;
                                      auto resmesh = SmoothingStage::synchronous(stage,colorgrid);
                                      ////////////////////////////////
                                      // todo: reduce latency here...
                                      ////////////////////////////////
                                      auto gfxop = [=]() {
                                        context->scheduleOnBeginFrame([=]() {
                                          resmesh->updateRigidPrim(prim, colorgrid, context);
                                        });
                                      };
                                      opq::mainSerialQueue()->enqueue(gfxop);
                                    };
                                    opq::concurrentQueue()->enqueue(op);
                                },
                                py::arg("conn"),
                                py::arg("colorgrid"),
                                py::arg("num_stages"),
                                py::arg("prim"),
                                py::arg("context"),
                                py::arg("validate") = false)
                            //////////////////////////////////////////////////
                            .def(
                                "smoothedWithColorGrid",
                                [](micromesh_ptr_t mesh,              //
                                   micromesh_connectivity_ptr_t conn, //
                                   vdb_vec3grid_ptr_t colorgrid,      //
                                   int num_stages,                    //
                                   umesh_rprim_ptr_t prim,
                                   ctx_t context,
                                   bool validate = false) { //
                                  auto stage = std::make_shared<SmoothingStage>();
                                  stage->mesh_inp = mesh;
                                  // Clone connectivity at entry point so each smoothing chain is isolated
                                  stage->conn = std::make_shared<MicroMeshConnectivity>(*conn);
                                  stage->prim = prim;
                                  stage->context.assign(context);
                                  stage->count = num_stages;
                                  stage->_validate = validate;
                                  SmoothingStage::synchronous(stage,colorgrid);
                                },
                                py::arg("conn"),
                                py::arg("colorgrid"),
                                py::arg("num_stages"),
                                py::arg("prim"),
                                py::arg("context"),
                                py::arg("validate") = false);
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
        py::gil_scoped_release release;
        auto prim = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t>();
        prim->fromSubMesh(*submesh, context.get());
        return prim;
      }))
      .def(
          "fromSubMesh",                                   //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             meshutil::submesh_ptr_t submesh,              //
             ctx_t context) {                              //
            py::gil_scoped_release release;
            prim->fromSubMesh(*submesh, context.get());
          })
      .def(
          "updateWithMicroMesh",                                 //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             micromesh_ptr_t micromesh,                    //
             ctx_t context,                                //
             crcstring_ptr_t primtype) {
             py::gil_scoped_release release;
             auto ptype = primtype ? PrimitiveType(primtype->hashed()) : PrimitiveType::TRIANGLES;
             micromesh->updateRigidPrim(prim, nullptr, context, ptype);
          },
          py::arg("micromesh"),
          py::arg("context"),
          py::arg("primitive_type") = nullptr)
      .def(
          "fromVertsAndFacesDict",                         //
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, //
             py::object verts,                             //
             py::list faces,                               //
             ctx_t context,                                //
             crcstring_ptr_t primtype) {
            ////////////////////////////////////////////
            py::gil_scoped_release release;
            auto micromesh = std::make_shared<MicroMesh>(verts, faces);
            auto ptype = primtype ? PrimitiveType(primtype->hashed()) : PrimitiveType::TRIANGLES;
            micromesh->updateRigidPrim(prim, nullptr, context, ptype);
          },
          "Create rigid primitive from vertices (list or numpy array (N,3) float32) and faces",
          py::arg("verts"), py::arg("faces"), py::arg("context"), py::arg("primitive_type") = nullptr)
      .def("renderEML", [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, ctx_t context) { //
        prim->renderEML(context.get());
      })
      .def(
          "fromArrays",
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim,
             py::buffer positions,   // (N,3) float32
             py::buffer normals,     // (N,3) float32
             py::buffer binormals,   // (N,3) float32
             py::buffer uvs,         // (N,2) float32
             py::buffer colors,      // (N,4) uint8 RGBA, or empty
             py::buffer indices,     // (M,) uint32
             ctx_t context) {
            auto pos_info = positions.request();
            auto nrm_info = normals.request();
            auto bin_info = binormals.request();
            auto uv_info  = uvs.request();
            auto clr_info = colors.request();
            auto idx_info = indices.request();
            py::gil_scoped_release release;
            int num_verts = pos_info.shape[0];
            int num_indices = idx_info.shape[0];
            bool has_colors = (clr_info.size > 0);
            auto* pos_ptr = static_cast<const float*>(pos_info.ptr);
            auto* nrm_ptr = static_cast<const float*>(nrm_info.ptr);
            auto* bin_ptr = static_cast<const float*>(bin_info.ptr);
            auto* uv_ptr  = static_cast<const float*>(uv_info.ptr);
            auto* clr_ptr = has_colors ? static_cast<const uint8_t*>(clr_info.ptr) : nullptr;
            auto* idx_ptr = static_cast<const uint32_t*>(idx_info.ptr);

            auto GBI = context->GBI();
            using vtx_t = SVtxV12N12B12T8C4;
            prim->_gpuClusters.clear();
            auto cluster = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t::PrimGroupCluster>();
            auto vtxbuf = std::make_shared<lev2::StaticVertexBuffer<vtx_t>>(num_verts, 0);
            auto idxbuf = std::make_shared<lev2::StaticIndexBuffer<uint32_t>>(num_indices);
            cluster->_vtxbuffer = vtxbuf;
            auto PG = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t::PrimitiveGroup>();
            cluster->_primgroups.push_back(PG);
            PG->_primtype = lev2::PrimitiveType::TRIANGLES;
            PG->_idxbuffer = idxbuf;
            prim->_gpuClusters.push_back(cluster);

            auto vtxptr = GBI->LockVB(*vtxbuf.get(), 0, num_verts);
            auto* typed_verts = (vtx_t*)vtxptr;
            for (int i = 0; i < num_verts; i++) {
              auto& v = typed_verts[i];
              v._position = fvec3(pos_ptr[i*3], pos_ptr[i*3+1], pos_ptr[i*3+2]);
              v._normal   = fvec3(nrm_ptr[i*3], nrm_ptr[i*3+1], nrm_ptr[i*3+2]);
              v._binormal = fvec3(bin_ptr[i*3], bin_ptr[i*3+1], bin_ptr[i*3+2]);
              v._uv       = fvec2(uv_ptr[i*2], uv_ptr[i*2+1]);
              v._color    = has_colors
                          ? (uint32_t(clr_ptr[i*4]) | (uint32_t(clr_ptr[i*4+1])<<8) | (uint32_t(clr_ptr[i*4+2])<<16) | (uint32_t(clr_ptr[i*4+3])<<24))
                          : 0xFFFFFFFF;
            }
            GBI->UnLockVB(*vtxbuf.get());

            auto idxptr = GBI->LockIB(*idxbuf.get(), 0, num_indices);
            auto* typed_idx = (uint32_t*)idxptr;
            for (int i = 0; i < num_indices; i++) {
              typed_idx[i] = idx_ptr[i];
            }
            GBI->UnLockIB(*idxbuf.get());
          },
          "Create rigid primitive directly from vertex attribute arrays, bypassing mesh processing",
          py::arg("positions"), py::arg("normals"), py::arg("binormals"),
          py::arg("uvs"), py::arg("colors"), py::arg("indices"), py::arg("context"))
      .def(
          "createInstancedNode",
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim,
             int count,
             std::string named,
             scenegraph::layer_ptr_t layer,
             material_ptr_t material) -> scenegraph::drawable_node_ptr_t {
            using drw_t = meshutil::InstancedRigidPrimitiveDrawable<SVtxV12N12B12T8C4>;
            auto drw = std::make_shared<drw_t>();
            drw->bindPrimitive(prim, material);
            drw->resize(count);
            auto instdata = drw->_instancedata;
            for (int i = 0; i < count; i++) {
              instdata->_worldmatrices[i].compose(fvec3(0, 0, 0), fquat(), 0.0f);
              instdata->_modcolors[i] = fvec4(1, 1, 1, 1);
            }
            auto node = layer->createDrawableNode(named, drw);
            return node;
          },
          py::arg("count"), py::arg("name"), py::arg("layer"), py::arg("material"));
} // void pyinit_gfx_rigidprim(py::module& module_lev2) {
} // namespace ork::lev2
