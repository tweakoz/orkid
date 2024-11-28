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

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_rigidprim(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto micromesh_type = py::class_<MicroMesh,micromesh_ptr_t>(module_lev2, "MicroMesh")
    //////////////////////////////////////////////////
    .def_static("fromVertAndFaceLists", [](py::list vert_list, py::list face_list) -> micromesh_ptr_t {
      return std::make_shared<MicroMesh>(vert_list, face_list);
    })
    //////////////////////////////////////////////////
    .def_property_readonly("vertices", [](micromesh_ptr_t mesh) -> py::list {
      auto verts = py::list();
      for( auto& vtx : mesh->_vertices ){
        verts.append(fvec3(vtx.x,vtx.y,vtx.z));
      }
      return verts;
    })
    //////////////////////////////////////////////////
    .def_property_readonly("tris", [](micromesh_ptr_t mesh) -> py::list {
      auto tris = py::list();
      for( auto& face : mesh->_tris ){
        tris.append(face);
      }
      return tris;
    })
    //////////////////////////////////////////////////
    .def_property_readonly("quads", [](micromesh_ptr_t mesh) -> py::list {
      auto quads = py::list();
      for( auto& face : mesh->_quads ){
        quads.append(face);
      }
      return quads;
    })
    //////////////////////////////////////////////////
    .def_property_readonly("vertexConnectivity", [](micromesh_ptr_t mesh) -> micromesh_connectivity_ptr_t {
      return mesh->computeVertexConnectivity();
    });
  /////////////////////////////////////////////////////////////////////////////////
  auto micromesh_conn_type = py::class_<MicroMeshConnectivity,micromesh_connectivity_ptr_t>(module_lev2, "MicroMeshConnectivity");
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
              });
  type_codec->registerStdCodec<meshutil::rigidprimitive_ptr_t>(rprimbase_t);
  /////////////////////////////////////////////////////////////////////////////////
  using rigidprim_t     = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  using rigidprim_ptr_t = std::shared_ptr<rigidprim_t>;
  py::class_<rigidprim_t, meshutil::RigidPrimitiveBase, rigidprim_ptr_t>(module_lev2, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](meshutil::submesh_ptr_t submesh, ctx_t context) {
        auto prim = std::make_shared<rigidprim_t>();
        prim->fromSubMesh(*submesh, context.get());
        return prim;
      }))
      .def(
          "fromSubMesh",
          [](rigidprim_ptr_t prim, meshutil::submesh_ptr_t submesh, ctx_t context) { prim->fromSubMesh(*submesh, context.get()); })
      .def(
          "fromVertsAndFacesDict",
          [](rigidprim_ptr_t prim, py::list verts, py::list faces, bool smooth, ctx_t context) { //
            ////////////////////////////////////////////
            auto micromesh = std::make_shared<MicroMesh>(verts, faces);
            auto conn      = micromesh->computeVertexConnectivity();
            ////////////////////////////////////////////
            if(smooth){
              for(int i=0; i<8; i++ ){
                micromesh = micromesh->smoothed(conn);
              }
            }
            ////////////////////////////////////////////
            int num_verts = micromesh->_vertices.size();
            int num_tris = micromesh->_tris.size();
            int num_quads = micromesh->_quads.size();
            int num_indices_required = num_tris * 3 + num_quads * 6;
            ////////////////////////////////////////////
            auto GBI = context->GBI();
            prim->_gpuClusters.clear();
            auto cluster        = std::make_shared<rigidprim_t::PrimGroupCluster>();
            auto vtxbuf         = std::make_shared<lev2::StaticVertexBuffer<SVtxV12N12B12T8C4>>(num_verts, 0);
            auto idxbuf         = std::make_shared<lev2::StaticIndexBuffer<uint32_t>>(num_indices_required);
            cluster->_vtxbuffer = vtxbuf;
            auto PG             = std::make_shared<rigidprim_t::PrimitiveGroup>();
            cluster->_primgroups.push_back(PG);
            PG->_primtype  = lev2::PrimitiveType::TRIANGLES;
            PG->_idxbuffer = idxbuf;
            prim->_gpuClusters.push_back(cluster);
            //////////////////////////////////////////////////////////////
            auto normals = micromesh->computeNormals(conn);
            //////////////////////////////////////////////////////////////
            auto vtxptr            = GBI->LockVB(*vtxbuf.get(), 0, num_verts);
            auto typed_vertex_base = (SVtxV12N12B12T8C4*)vtxptr;
            int ivtx = 0;
            for (auto vtx_in : micromesh->_vertices) {
              auto& vertex_out     = typed_vertex_base[ivtx];
              vertex_out._position = vtx_in;
              const auto& N   = normals[ivtx];
              vertex_out._normal   = N;
              uint32_t color       = 0;
              color |= uint32_t((N.x * 0.5f + 0.5f) * 255.0f);
              color |= uint32_t((N.y * 0.5f + 0.5f) * 255.0f) << 8;
              color |= uint32_t((N.z * 0.5f + 0.5f) * 255.0f) << 16;
              vertex_out._color = color;
              ivtx++;
            }
            //////////////////////////////////////////////////////////////
            int oidx              = 0;
            auto idxptr           = GBI->LockIB(*idxbuf.get(), 0, num_indices_required);
            auto typed_indices = (uint32_t*)idxptr;

            for( auto t : micromesh->_tris ){
              typed_indices[oidx++] = t[2];
              typed_indices[oidx++] = t[1];
              typed_indices[oidx++] = t[0];
            }
            for( auto q : micromesh->_quads ){
              typed_indices[oidx++] = q[2];
              typed_indices[oidx++] = q[1];
              typed_indices[oidx++] = q[0];
              typed_indices[oidx++] = q[2];
              typed_indices[oidx++] = q[0];
              typed_indices[oidx++] = q[3];
            }
            OrkAssert(oidx == num_indices_required);
            // printf("oidx<%d> num_indices_required<%d>\n", oidx, num_indices_required);
            GBI->UnLockIB(*idxbuf.get());
            GBI->UnLockVB(*vtxbuf.get());
            //////////////////////////////////////////////////////////////
            // prim->fromData(primdata, context.get());
          })
      .def("renderEML", [](rigidprim_ptr_t prim, ctx_t context) { //
        prim->renderEML(context.get());
      });
} // void pyinit_gfx_rigidprim(py::module& module_lev2) {
} // namespace ork::lev2