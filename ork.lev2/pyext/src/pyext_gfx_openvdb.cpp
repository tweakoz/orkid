////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include "_vdb_impl.h"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_openvdb(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb = module_lev2.def_submodule("vdb", "OrkidOpenVDBBridge");
  auto ax   = ovdb.def_submodule("ax", "OrkidOpenVDBAxBridge");

  auto grid_type = py::class_<vdb_basegrid_t, vdb_basegrid_ptr_t>(ovdb, "BaseGrid")
                       .def("activeVoxelCount", [](vdb_basegrid_ptr_t grid) -> uint64_t { return grid->activeVoxelCount(); });
  type_codec->registerStdCodec<vdb_basegrid_ptr_t>(grid_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct citer_proxy {
    openvdb::FloatGrid::ValueOnCIter iter;
    citer_proxy(openvdb::FloatGrid::ValueOnCIter iter)
        : iter(iter) {
    }
    std::pair<openvdb::Coord, float> next() {
      if (iter) {
        auto coord = iter.getCoord();
        auto value = *iter;
        ++iter;
        return std::make_pair(coord, value);
      } else {
        throw py::stop_iteration();
      }
    }
  };
  using citer_proxy_ptr = std::shared_ptr<citer_proxy>;
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_fgrid_iter_type = py::class_<citer_proxy, citer_proxy_ptr>(ovdb, "FloatGridIter")
                                  .def("__iter__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { return iter; })
                                  .def("__next__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { //
                                    auto next = std::make_shared<citer_proxy>(iter->iter);
                                    return next;
                                  });
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::Transform
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_xform_type = py::class_<vdb_transform_t, vdb_transform_ptr_t>(ovdb, "Transform")
                             .def_static("create", [](float scale) -> vdb_transform_ptr_t {
                               py::gil_scoped_release release;
                               auto xform = std::make_shared<openvdb::math::Transform>();
                               xform->postScale(scale);
                               return xform;
                             })
                             .def("__repr__", [](vdb_transform_ptr_t xform) -> std::string {
                               std::ostringstream oss;
                               //oss << "Transform(" << xform->getAffineMap() << ")";
                               return oss.str();
                             });
  type_codec->registerStdCodec<vdb_transform_ptr_t>(ovdb_xform_type);
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_fgrid_type =
      py::class_<vdb_floatgrid_t, vdb_basegrid_t, vdb_floatgrid_ptr_t>(ovdb, "FloatGrid")
          .def_static(
              "create",
              [](std::string name, vdb_transform_ptr_t xform, float background) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto grid = std::make_shared<openvdb::FloatGrid>(background);
                grid->setName(name);
                grid->setTransform(xform);
                grid->setGridClass(openvdb::GRID_LEVEL_SET);
                return grid;
              })
          .def_static(
              "createLevelSetSphere",
              [](std::string name, float radius, fvec3 center, float vxlsize, float hwidth) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto grid = openvdb::tools::createLevelSetSphere<vdb_floatgrid_t>(
                    radius, openvdb::Vec3f(center.x, center.y, center.z), vxlsize, hwidth);
                grid->setName(name);
                // createLevelSetSphere (float radius, const openvdb::Vec3f &center, float voxelSize, float
                // halfWidth=float(LEVEL_SET_HALF_WIDTH), InterruptT *interrupt=nullptr, bool threaded=true)
                return grid;
              })
          ///////////////////////////////////////////////////////
          .def(
              "fill",
              [](vdb_floatgrid_ptr_t grid, fvec3 center, float radius, float value) {
                py::gil_scoped_release release;
                openvdb::CoordBBox bbox;
                auto coord_va = openvdb::Vec3f(center.x-radius, center.y-radius, center.z-radius);
                auto coord_vb = openvdb::Vec3f(center.x+radius, center.y+radius, center.z+radius);
                auto coord_ia = grid->worldToIndex(coord_va);
                auto coord_ib = grid->worldToIndex(coord_vb);
                bbox.expand(openvdb::Coord(coord_va.x(), coord_va.y(), coord_va.z()));
                bbox.expand(openvdb::Coord(coord_vb.x(), coord_vb.y(), coord_vb.z()));
                grid->fill(bbox, value);
              })
          ///////////////////////////////////////////////////////
          .def_property(
              "background",
              [](vdb_floatgrid_ptr_t grid) -> float { return grid->background(); },
              [](vdb_floatgrid_ptr_t grid, float value) {
                size_t grainSize = 32;
                openvdb::tools::changeLevelSetBackground(grid->tree(), value, true, grainSize);
              })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "onValueSequence",
              [](vdb_floatgrid_ptr_t grid) -> citer_proxy_ptr {
                auto iter = std::make_shared<citer_proxy>(grid->cbeginValueOn());
                return iter;
              })
           .def_property_readonly(
              "xform",
              [](vdb_floatgrid_ptr_t grid) -> vdb_transform_ptr_t {
                return grid->transformPtr();
              })
           .def_property_readonly(
              "clone",
              [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
                return std::make_shared<vdb_floatgrid_t>(*grid);
              })
          ///////////////////////////////////////////////////////
          .def(
              "scatterVoxels",
              [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result = grid->copyWithNewTree();
                std::random_device rand_dev;
                std::default_random_engine e1(rand_dev());
                std::uniform_int_distribution<int> uniform_dist(0, 2);
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    float value = *voxelIter;
                    auto icoord = voxelIter.getCoord();
                    int itx     = uniform_dist(e1) - 1;
                    int ity     = uniform_dist(e1) - 1;
                    int itz     = uniform_dist(e1) - 1;
                    icoord      = icoord + openvdb::Coord(itx, ity, itz);
                    result->tree().setValueOn(icoord, value);
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "scatterVoxels2",
              [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result            = grid->copyWithNewTree();
                static auto cq         = std::make_shared<cq_t>();
                int num_points         = grid->tree().activeLeafVoxelCount();
                std::atomic<int> count = num_points;
                std::random_device rand_dev;

                for (auto itl0 = grid->tree().cbeginRootChildren(); itl0; ++itl0) {
                  auto op = [=, &rand_dev]() {
                    std::default_random_engine e1(rand_dev());
                    std::uniform_int_distribution<int> uniform_dist(0, 2);
                    FloatVoxel fv;
                    for (auto itl1 = itl0->cbeginChildOn(); itl1; ++itl1) {
                      for (auto itl2 = itl1->cbeginChildOn(); itl2; ++itl2) {
                        const float* src_data = itl2->buffer().data();
                        for (auto it_vox = itl2->cbeginValueOn(); it_vox; ++it_vox) {
                          int itx  = uniform_dist(e1) - 1;
                          int ity  = uniform_dist(e1) - 1;
                          int itz  = uniform_dist(e1) - 1;
                          fv.coord = it_vox.getCoord() + openvdb::Coord(itx, ity, itz);
                          auto idx = it_vox.pos();
                          fv.value = src_data[idx];
                          cq->push(fv);
                        }
                      }
                    }
                  };
                  opq::concurrentQueue()->enqueue(op);
                }
                FloatVoxel fv;
                while ((count.load() > 0)) {
                  if (cq->try_pop(fv)) {
                    result->tree().setValueOn(fv.coord, fv.value);
                    count--;
                  } else {
                    sched_yield();
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "translatedVoxels",
              [](vdb_floatgrid_ptr_t grid, fvec3 trans) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result = grid->copyWithNewTree();
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    float value = *voxelIter;
                    auto icoord = voxelIter.getCoord();
                    icoord      = icoord + openvdb::Coord(trans.x, trans.y, trans.z);
                    result->tree().setValueOn(icoord, value);
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "setVoxel",
              [](vdb_floatgrid_ptr_t grid, fvec3 coord, float value) {
                py::gil_scoped_release release;
                grid->tree().setValue(openvdb::Coord(coord.x, coord.y, coord.z), value);
              })
          ///////////////////////////////////////////////////////
          .def(
              "worldToIndex",
              [](vdb_floatgrid_ptr_t grid, fvec3 coord) -> fvec3 {
                py::gil_scoped_release release;
                auto index = grid->transform().worldToIndex(openvdb::Vec3f(coord.x, coord.y, coord.z));
                return fvec3(index.x(), index.y(), index.z());
              })
          ///////////////////////////////////////////////////////
          .def(
              "drawLineI",
              [](vdb_floatgrid_ptr_t grid, fvec3 start, fvec3 end, float value) {
                py::gil_scoped_release release;
                // Calculate differences
                float dx = end.x - start.x;
                float dy = end.y - start.y;
                float dz = end.z - start.z;

                // Determine the number of steps needed
                float steps = std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz)});

                // Calculate the increment in each coordinate
                float Xinc = dx / steps;
                float Yinc = dy / steps;
                float Zinc = dz / steps;

                // Initialize starting point
                float x = start.x;
                float y = start.y;
                float z = start.z;

                // Generate points along the line
                for (int i = 0; i <= steps; i++) {
                  int ix = static_cast<int>(std::round(x));
                  int iy = static_cast<int>(std::round(y));
                  int iz = static_cast<int>(std::round(z));
                  grid->tree().setValue(openvdb::Coord(ix, iy, iz), value);
                  x += Xinc;
                  y += Yinc;
                  z += Zinc;
                }
              })
          ///////////////////////////////////////////////////////
          .def("toQuads", [](vdb_floatgrid_ptr_t grid, float isovalue) -> py::dict {
            std::vector<openvdb::Vec3s> points;
            std::vector<openvdb::Vec4I> quads;
            std::vector<openvdb::Vec3I> tris;
            {
              py::gil_scoped_release release;

              bool relax       = false;
              float adaptivity = 0.0f;

              openvdb::tools::volumeToMesh(*grid, points, tris, quads, isovalue, adaptivity, relax);
            }
            auto vertices = py::list();
            auto indices  = py::list();
            for (auto& point : points) {
              auto world = grid->transform().indexToWorld(point);
              vertices.append(fvec3(point.x(), point.y(), point.z()));
            }
            for (auto& quad : quads) {
              indices.append(4);
              indices.append(quad[3]);
              indices.append(quad[2]);
              indices.append(quad[1]);
              indices.append(quad[0]);
            }
            for (auto& tri : tris) {
              indices.append(3);
              indices.append(tri[2]);
              indices.append(tri[1]);
              indices.append(tri[0]);
            }

            auto result        = py::dict();
            result["vertices"] = vertices;
            result["faces"]    = indices;
            return result;
          });
  type_codec->registerStdCodec<vdb_floatgrid_ptr_t>(ovdb_fgrid_type);
  /////////////////////////////////////////////////////////////////////////////////
  // AX volume executable
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_ax_ve_type = py::class_<vdb_volume_exec_t, vdb_volume_exec_ptr_t>(ax, "VolumeExecutable")
                             .def_static(
                                 "compile",
                                 [](std::string code, vdb_custom_data_ptr_t cdata = nullptr) -> vdb_volume_exec_ptr_t {
                                   py::gil_scoped_release release;
                                   openvdb::ax::Compiler compiler;
                                   return compiler.compile<vdb_volume_exec_t>(code, cdata);
                                 })
                             .def("executeOnGrid", [](vdb_volume_exec_ptr_t ve, vdb_floatgrid_ptr_t grid) {
                               py::gil_scoped_release release;
                               ve->execute(*grid);
                             });
  type_codec->registerStdCodec<vdb_volume_exec_ptr_t>(ovdb_ax_ve_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cdata_type = py::class_<vdb_custom_data_t, vdb_custom_data_ptr_t>(ax, "CustomData")
                        .def(py::init<>())
                        .def("set", [](vdb_custom_data_ptr_t cdata, std::string key, py::object value) {
                          if (py::isinstance<py::int_>(value)) {
                            auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<int>>(key);
                            typed->setValue(py::cast<int>(value));
                          } else if (py::isinstance<py::float_>(value)) {
                            auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<float>>(key);
                            // printf("set float key<%s> value<%f>\n", key.c_str(), py::cast<float>(value));
                            typed->setValue(py::cast<float>(value));
                          } else {
                            OrkAssert(false); // unsupported type
                          }
                        });
  type_codec->registerStdCodec<vdb_custom_data_ptr_t>(cdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_v3grid_type =
      py::class_<vdb_vec3grid_t, vdb_basegrid_t, vdb_vec3grid_ptr_t>(ovdb, "Vec3FGrid")
          .def_static(
              "create",
              [](std::string name, vdb_transform_ptr_t xform, fvec3 background) -> vdb_vec3grid_ptr_t {
                py::gil_scoped_release release;
                auto bg   = openvdb::Vec3f(background.x, background.y, background.z);
                auto grid = std::make_shared<vdb_vec3grid_t>(bg);
                grid->setName(name);
                grid->setTransform(xform);
                //grid->setGridClass(openvdb::GRID_LEVEL_SET);
                return grid;
              })
          ///////////////////////////////////////////////////////
          .def(
              "fill",
              [](vdb_vec3grid_ptr_t grid, fvec3 center, float radius, fvec3 value) {
                py::gil_scoped_release release;
                openvdb::CoordBBox bbox;
                //printf("fill center<%f %f %f> radius<%f> value<%f %f %f>\n", center.x, center.y, center.z, radius, value.x, value.y, value.z);
                auto coord_va = openvdb::Vec3f(center.x-radius, center.y-radius, center.z-radius);
                auto coord_vb = openvdb::Vec3f(center.x+radius, center.y+radius, center.z+radius);
                auto coord_ia = grid->worldToIndex(coord_va);
                auto coord_ib = grid->worldToIndex(coord_vb);

                bbox.expand(openvdb::Coord(coord_va.x(), coord_va.y(), coord_va.z()));
                bbox.expand(openvdb::Coord(coord_vb.x(), coord_vb.y(), coord_vb.z()));
                grid->fill(bbox, openvdb::Vec3f(value.x, value.y, value.z));
              })
          ///////////////////////////////////////////////////////
          .def_property(
              "background",
              [](vdb_vec3grid_ptr_t grid) -> fvec3 { //
                auto bg = grid->background();
                return fvec3(bg.x(), bg.y(), bg.z());
              },
              [](vdb_vec3grid_ptr_t grid, fvec3 value) {
                size_t grainSize = 32;
                auto as_vec3f    = openvdb::Vec3f(value.x, value.y, value.z);
                //openvdb::tools::changeLevelSetBackground(grid->tree(), as_vec3f, true, grainSize);
              })
           .def_property_readonly(
              "clone",
              [](vdb_vec3grid_ptr_t grid) -> vdb_vec3grid_ptr_t {
                return std::make_shared<vdb_vec3grid_t>(*grid);
              });
  type_codec->registerStdCodec<vdb_vec3grid_ptr_t>(ovdb_v3grid_type);
}
} // namespace ork::lev2
