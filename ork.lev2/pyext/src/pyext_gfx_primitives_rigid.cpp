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
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

#include "pyext.h"
#include <pybind11/numpy.h>
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

/////////////////////////////////////////////////
// Geometry channel <-> numpy. geom_channel_buffer exposes a typed channel's
// contiguous AOS storage through the buffer protocol (float-family -> f32,
// int -> i32; readonly=false), so np.asarray(channel) is a ZERO-COPY,
// WRITEABLE view (numpy keeps the channel alive via the buffer reference).
/////////////////////////////////////////////////
template <typename T> static py::buffer_info geom_channel_buffer(meshutil::GeomChannel<T>& c) {
  constexpr bool isint    = std::is_same<T, int>::value;
  using scalar_t          = typename std::conditional<isint, int, float>::type;
  constexpr ssize_t ssz   = ssize_t(sizeof(scalar_t));
  constexpr ssize_t ncomp = ssize_t(sizeof(T) / sizeof(scalar_t));
  ssize_t n               = ssize_t(c._data.size());
  std::string fmt         = py::format_descriptor<scalar_t>::format();
  void* ptr               = c._data.data();
  if (std::is_same<T, fmtx4>::value) // (n,4,4)
    return py::buffer_info(ptr, ssz, fmt, 3, {n, ssize_t(4), ssize_t(4)}, {ssize_t(sizeof(T)), ssize_t(4) * ssz, ssz}, false);
  if (ncomp == 1) // (n,)
    return py::buffer_info(ptr, ssz, fmt, 1, {n}, {ssize_t(sizeof(T))}, false);
  return py::buffer_info(ptr, ssz, fmt, 2, {n, ncomp}, {ssize_t(sizeof(T)), ssz}, false); // (n,comps)
}

// np.asarray over the channel's buffer protocol -> zero-copy writeable view.
static py::object geom_channel_ndarray(meshutil::geomchannel_ptr_t ch) {
  return py::module::import("numpy").attr("asarray")(py::cast(ch));
}

// Bulk authoring: create / replace a channel in `attrs` from a numpy/list,
// inferring float/int + tuple size from the array shape/dtype. Shared by
// GeomAttributes.set / __setitem__ and the Geometry point-attr sugar.
static void geom_attrs_set(meshutil::GeomAttributes& attrs, const std::string& name, py::object data) {
  py::array arr = py::array::ensure(data);
  if (not arr)
    throw std::runtime_error("GeomAttributes.set: data must be array-like (list or numpy)");
  py::buffer_info info = arr.request();
  if (info.ndim < 1)
    throw std::runtime_error("GeomAttributes.set: data must have at least one dimension");
  ssize_t n     = info.shape[0];
  ssize_t comps = 1;
  for (ssize_t d = 1; d < info.ndim; d++)
    comps *= info.shape[d];
  char kind = arr.dtype().kind();
  if (kind == 'f') {
    auto a           = py::array_t<float, py::array::c_style | py::array::forcecast>(arr);
    const float* src = a.data();
    switch (comps) {
      case 1: { auto ch = attrs.createChannel<float>(name); ch->_data.assign(src, src + n); break; }
      case 2: { auto ch = attrs.createChannel<fvec2>(name); ch->_data.resize(n); std::memcpy(ch->_data.data(), src, size_t(n) * sizeof(fvec2)); break; }
      case 3: { auto ch = attrs.createChannel<fvec3>(name); ch->_data.resize(n); std::memcpy(ch->_data.data(), src, size_t(n) * sizeof(fvec3)); break; }
      case 4: { auto ch = attrs.createChannel<fvec4>(name); ch->_data.resize(n); std::memcpy(ch->_data.data(), src, size_t(n) * sizeof(fvec4)); break; }
      case 16: { auto ch = attrs.createChannel<fmtx4>(name); ch->_data.resize(n); std::memcpy(ch->_data.data(), src, size_t(n) * sizeof(fmtx4)); break; }
      default: throw std::runtime_error("GeomAttributes.set: unsupported float tuple size (expected 1/2/3/4/16)");
    }
  } else if (kind == 'i' or kind == 'u') {
    if (comps != 1)
      throw std::runtime_error("GeomAttributes.set: only scalar int channels are supported");
    auto a         = py::array_t<int, py::array::c_style | py::array::forcecast>(arr);
    const int* src = a.data();
    auto ch        = attrs.createChannel<int>(name);
    ch->_data.assign(src, src + n);
  } else {
    throw std::runtime_error("GeomAttributes.set: dtype must be float or int");
  }
}

/////////////////////////////////////////////////
// Per-type Geometry channel binding — strict typed element accessors
// (get/set/append over fvec3/fvec4/... directly, no py::object) + a zero-copy
// numpy view via the buffer protocol.
/////////////////////////////////////////////////
template <typename T> static void bind_geom_channel(py::module& module_lev2, const char* pyname) {
  namespace mu = meshutil;
  py::class_<mu::GeomChannel<T>, mu::GeomChannelBase, mu::geomchannel_typed_ptr_t<T>>(module_lev2, pyname, py::buffer_protocol())
      .def_buffer([](mu::GeomChannel<T>& c) -> py::buffer_info { return geom_channel_buffer<T>(c); })
      .def("__len__", [](mu::geomchannel_typed_ptr_t<T> c) -> size_t { return c->_data.size(); })
      .def("size", [](mu::geomchannel_typed_ptr_t<T> c) -> size_t { return c->_data.size(); })
      .def(
          "resize",
          [](mu::geomchannel_typed_ptr_t<T> c, size_t n) { c->_data.resize(n); },
          py::arg("count"))
      .def(
          "get",
          [](mu::geomchannel_typed_ptr_t<T> c, int index) -> T { return c->_data.at(index); },
          py::arg("index"))
      .def(
          "set",
          [](mu::geomchannel_typed_ptr_t<T> c, int index, T value) { c->_data.at(index) = value; },
          py::arg("index"),
          py::arg("value"))
      .def(
          "append",
          [](mu::geomchannel_typed_ptr_t<T> c, T value) { c->_data.push_back(value); },
          py::arg("value"))
      .def("array", [](mu::geomchannel_typed_ptr_t<T> c) -> py::object { return geom_channel_ndarray(c); });
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
          .def(
              "createDrawableData",
              [](meshutil::rigidprimitive_ptr_t prim,                                  //
                 material_ptr_t material) -> meshutil::rigidprimitive_drawdata_ptr_t { //
                // Attach via the MATERIAL (not a baked pipeline) so the drawable
                // does per-pass technique selection — a depth pipeline in the
                // depth-prepass, the forward-lighting pipeline in the forward
                // pass. A baked FORWARD_PBR pipeline would run the forward
                // lighting lambda during the depth-prepass and assert on the
                // unbound PBR_COMMON frame-property.
                auto drwdata        = std::make_shared<meshutil::RigidPrimitiveDrawableData>();
                drwdata->_material  = material;
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
      .def("renderEML", [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim, ctx_t context) { //
        prim->renderEML(context.get());
      })
      .def(
          "createInstancedNode",
          [](meshutil::rigidprim_V12N12B12T8C4_ptr_t prim,
             int count,
             std::string named,
             scenegraph::layer_ptr_t layer,
             material_ptr_t material,
             bool cull) -> scenegraph::drawable_node_ptr_t {
            using drw_t = meshutil::InstancedRigidPrimitiveDrawable<SVtxV12N12B12T8C4>;
            auto drw = std::make_shared<drw_t>();
            drw->bindPrimitive(prim, material);
            drw->enableCull(cull); // GPU frustum cull -> indirect draw (per-VP, onPreRender)
            drw->resize(count);
            auto instdata = drw->_instancedata;
            for (int i = 0; i < count; i++) {
              instdata->_worldmatrices[i].compose(fvec3(0, 0, 0), fquat(), 0.0f);
              if (not drw->_matrices_only) // _modcolors is empty in matrices-only (no per-instance color)
                instdata->_modcolors[i] = fvec4(1, 1, 1, 1);
            }
            auto node = layer->createDrawableNode(named, drw);
            return node;
          },
          py::arg("count"), py::arg("name"), py::arg("layer"), py::arg("material"),
          py::arg("cull") = false);
  /////////////////////////////////////////////////////////////////////////////////
  // Geometry — standalone attribute-based geometry container.
  // Channels are first-class strictly-typed objects with zero-copy numpy views
  // (buffer protocol). Attributes are owner-scoped (point/vertex/prim/detail);
  // Geometry exposes those sets plus __getitem__/__setitem__ sugar over its
  // POINT attributes, clone(), an .ogeo chunkfile codec, and a MicroMesh
  // adapter.
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<meshutil::GeomChannelType>(module_lev2, "GeomChannelType")
      .value("FLOAT", meshutil::GeomChannelType::FLOAT)
      .value("INT", meshutil::GeomChannelType::INT)
      .value("VEC2", meshutil::GeomChannelType::VEC2)
      .value("VEC3", meshutil::GeomChannelType::VEC3)
      .value("VEC4", meshutil::GeomChannelType::VEC4)
      .value("QUAT", meshutil::GeomChannelType::QUAT)
      .value("MTX4", meshutil::GeomChannelType::MTX4);
  auto geomchannel_base_t =
      py::class_<meshutil::GeomChannelBase, meshutil::geomchannel_ptr_t>(module_lev2, "GeomChannel")
          .def("size", [](meshutil::geomchannel_ptr_t c) -> size_t { return c->count(); })
          .def("__len__", [](meshutil::geomchannel_ptr_t c) -> size_t { return c->count(); })
          .def_property_readonly("datatype", [](meshutil::geomchannel_ptr_t c) { return c->_datatype; });
  type_codec->registerStdCodec<meshutil::geomchannel_ptr_t>(geomchannel_base_t);
  bind_geom_channel<float>(module_lev2, "FloatChannel");
  bind_geom_channel<int>(module_lev2, "IntChannel");
  bind_geom_channel<fvec2>(module_lev2, "Vec2Channel");
  bind_geom_channel<fvec3>(module_lev2, "Vec3Channel");
  bind_geom_channel<fvec4>(module_lev2, "Vec4Channel");
  bind_geom_channel<fquat>(module_lev2, "QuatChannel");
  bind_geom_channel<fmtx4>(module_lev2, "Mtx4Channel");
  /////////////////////////////////////////////////
  // GeomAttributes — owner-scoped channel set (point / vertex / prim / detail).
  // __getitem__ returns a zero-copy writeable numpy view; __setitem__ bulk
  // creates/replaces the channel (tuple size & float/int inferred).
  /////////////////////////////////////////////////
  py::class_<meshutil::GeomAttributes>(module_lev2, "GeomAttributes")
      .def(
          "createChannel",
          [](meshutil::GeomAttributes& self, const std::string& name, meshutil::GeomChannelType t) -> meshutil::geomchannel_ptr_t {
            switch (t) {
              case meshutil::GeomChannelType::FLOAT: return self.createChannel<float>(name);
              case meshutil::GeomChannelType::INT:   return self.createChannel<int>(name);
              case meshutil::GeomChannelType::VEC2:  return self.createChannel<fvec2>(name);
              case meshutil::GeomChannelType::VEC3:  return self.createChannel<fvec3>(name);
              case meshutil::GeomChannelType::VEC4:  return self.createChannel<fvec4>(name);
              case meshutil::GeomChannelType::QUAT:  return self.createChannel<fquat>(name);
              case meshutil::GeomChannelType::MTX4:  return self.createChannel<fmtx4>(name);
            }
            return nullptr;
          },
          py::arg("name"),
          py::arg("type"))
      .def(
          "channel",
          [](meshutil::GeomAttributes& self, const std::string& name) -> meshutil::geomchannel_ptr_t { return self.channel(name); },
          py::arg("name"))
      .def(
          "hasChannel",
          [](meshutil::GeomAttributes& self, const std::string& name) { return self.hasChannel(name); },
          py::arg("name"))
      .def("__contains__", [](meshutil::GeomAttributes& self, const std::string& name) { return self.hasChannel(name); })
      .def(
          "removeChannel",
          [](meshutil::GeomAttributes& self, const std::string& name) { self.removeChannel(name); },
          py::arg("name"))
      .def("channelNames", [](meshutil::GeomAttributes& self) { return self.channelNames(); })
      .def("__len__", [](meshutil::GeomAttributes& self) { return self.numChannels(); })
      .def(
          "set",
          [](meshutil::GeomAttributes& self, const std::string& name, py::object data) { geom_attrs_set(self, name, data); },
          py::arg("name"),
          py::arg("data"))
      .def("__setitem__", [](meshutil::GeomAttributes& self, const std::string& name, py::object data) { geom_attrs_set(self, name, data); })
      .def(
          "get",
          [](meshutil::GeomAttributes& self, const std::string& name) -> py::object {
            auto ch = self.channel(name);
            if (not ch)
              return py::none();
            return geom_channel_ndarray(ch);
          },
          py::arg("name"))
      .def("__getitem__", [](meshutil::GeomAttributes& self, const std::string& name) -> py::object {
        auto ch = self.channel(name);
        if (not ch)
          throw py::key_error(name);
        return geom_channel_ndarray(ch);
      });
  /////////////////////////////////////////////////
  auto geometry_t =
      py::class_<meshutil::Geometry, meshutil::geometry_ptr_t>(module_lev2, "Geometry")
          .def(py::init<>())
          .def_property_readonly("point",  [](meshutil::geometry_ptr_t g) -> meshutil::GeomAttributes& { return g->_point; },  py::return_value_policy::reference_internal)
          .def_property_readonly("vertex", [](meshutil::geometry_ptr_t g) -> meshutil::GeomAttributes& { return g->_vertex; }, py::return_value_policy::reference_internal)
          .def_property_readonly("prim",   [](meshutil::geometry_ptr_t g) -> meshutil::GeomAttributes& { return g->_prim; },   py::return_value_policy::reference_internal)
          .def_property_readonly("detail", [](meshutil::geometry_ptr_t g) -> meshutil::GeomAttributes& { return g->_detail; }, py::return_value_policy::reference_internal)
          // point-attribute sugar: geo["P"] == geo.point["P"]
          .def("__getitem__", [](meshutil::geometry_ptr_t g, const std::string& name) -> py::object {
            auto ch = g->_point.channel(name);
            if (not ch)
              throw py::key_error(name);
            return geom_channel_ndarray(ch);
          })
          .def("__setitem__", [](meshutil::geometry_ptr_t g, const std::string& name, py::object data) { geom_attrs_set(g->_point, name, data); })
          .def("__contains__", [](meshutil::geometry_ptr_t g, const std::string& name) { return g->_point.hasChannel(name); })
          .def(
              "addPolys",
              [](meshutil::geometry_ptr_t geo, py::object indices, int sides) {
                auto a = py::array_t<int, py::array::c_style | py::array::forcecast>(py::array::ensure(indices));
                std::vector<int> v(a.data(), a.data() + a.size());
                geo->addPolys(v, sides);
              },
              py::arg("indices"),
              py::arg("sides") = 3)
          .def(
              "addPoly",
              [](meshutil::geometry_ptr_t geo, py::object point_indices) {
                auto a = py::array_t<int, py::array::c_style | py::array::forcecast>(py::array::ensure(point_indices));
                std::vector<int> v(a.data(), a.data() + a.size());
                geo->addPoly(v);
              },
              py::arg("point_indices"))
          .def("clone", [](meshutil::geometry_ptr_t geo) { return geo->clone(); })
          .def_property_readonly("num_points", [](meshutil::geometry_ptr_t geo) { return geo->numPoints(); })
          .def_property_readonly("num_vertices", [](meshutil::geometry_ptr_t geo) { return geo->numVertices(); })
          .def_property_readonly("num_prims", [](meshutil::geometry_ptr_t geo) { return geo->numPrims(); })
          .def_property_readonly("num_polys", [](meshutil::geometry_ptr_t geo) { return geo->numPolys(); })
          .def(
              "computeFaceNormal",
              [](meshutil::geometry_ptr_t geo, int ipoly) { return geo->computeFaceNormal(ipoly); },
              py::arg("ipoly"))
          .def(
              "write",
              [](meshutil::geometry_ptr_t geo, const std::string& path) {
                py::gil_scoped_release release;
                geo->writeChunkfile(file::Path(path.c_str()));
              },
              py::arg("path"))
          .def_static(
              "read",
              [](const std::string& path) -> meshutil::geometry_ptr_t {
                py::gil_scoped_release release;
                return meshutil::Geometry::readChunkfile(file::Path(path.c_str()));
              },
              py::arg("path"))
          .def("toMicroMesh", [](meshutil::geometry_ptr_t geo) -> micromesh_ptr_t {
            auto P = geo->_point.channelAs<fvec3>("P");
            if (not P)
              throw std::runtime_error("Geometry.toMicroMesh: requires a vec3 point 'P' channel");
            auto mesh       = std::make_shared<MicroMesh>();
            mesh->_vertices = P->_data;
            size_t nv       = mesh->_vertices.size();
            mesh->_colors.assign(nv, fvec4(1, 1, 1, 1));
            if (auto N = geo->_point.channelAs<fvec3>("N"); N and N->_data.size() == nv)
              mesh->_normals = N->_data;
            if (auto B = geo->_point.channelAs<fvec3>("binormal"); B and B->_data.size() == nv)
              mesh->_binormals = B->_data;
            if (auto uv = geo->_point.channelAs<fvec2>("uv"); uv and uv->_data.size() == nv)
              mesh->_uvs = uv->_data;
            if (auto cd = geo->_point.channelAs<fvec4>("Cd"); cd and cd->_data.size() == nv)
              mesh->_colors = cd->_data;
            int np = geo->numPolys();
            for (int i = 0; i < np; i++) {
              int cnt        = geo->polyVertexCount(i);
              const int* idx = geo->polyPointIndices(i);
              if (cnt == 2) {
                auto& l = mesh->_lines.emplace_back();
                l.push_back(idx[0]);
                l.push_back(idx[1]);
              } else if (cnt == 3) {
                auto& t = mesh->_tris.emplace_back();
                t.push_back(idx[0]);
                t.push_back(idx[1]);
                t.push_back(idx[2]);
              } else if (cnt == 4) {
                auto& q = mesh->_quads.emplace_back();
                q.push_back(idx[0]);
                q.push_back(idx[1]);
                q.push_back(idx[2]);
                q.push_back(idx[3]);
              } else {
                for (int k = 1; k + 1 < cnt; k++) { // fan-triangulate n-gons
                  auto& t = mesh->_tris.emplace_back();
                  t.push_back(idx[0]);
                  t.push_back(idx[k]);
                  t.push_back(idx[k + 1]);
                }
              }
            }
            mesh->_connectivity_dirty = true;
            return mesh;
          });
  type_codec->registerStdCodec<meshutil::geometry_ptr_t>(geometry_t);
} // void pyinit_gfx_rigidprim(py::module& module_lev2) {
} // namespace ork::lev2
