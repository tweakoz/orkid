////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/util/NullInterrupter.h>
#include <openvdb_ax/compiler/Logger.h>
#include <openvdb_ax/compiler/VolumeExecutable.h>
#include <openvdb_ax/compiler/Compiler.h>
#include <openvdb_ax/compiler/CustomData.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

using vdb_basegrid_t       = openvdb::GridBase;
using vdb_basegrid_ptr_t   = std::shared_ptr<vdb_basegrid_t>;
using vdb_floatgrid_t       = openvdb::FloatGrid;
using vdb_floatgrid_ptr_t   = std::shared_ptr<vdb_floatgrid_t>;
using vdb_volume_exec_t = openvdb::ax::VolumeExecutable;
using vdb_volume_exec_ptr_t = std::shared_ptr<vdb_volume_exec_t>;
using vdb_custom_data_t = openvdb::ax::CustomData;
using vdb_custom_data_ptr_t = std::shared_ptr<vdb_custom_data_t>;

struct FloatVoxel {
  openvdb::Coord coord;
  float value;
};
using cq_t = MpMcBoundedQueue<FloatVoxel,4<<20>;

void pyinit_gfx_openvdb(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb = module_lev2.def_submodule("vdb", "OrkidOpenVDBBridge");
  auto ax = ovdb.def_submodule("ax", "OrkidOpenVDBAxBridge");

  auto grid_type = 
    py::class_<vdb_basegrid_t, vdb_basegrid_ptr_t>(ovdb, "BaseGrid")
    .def("activeVoxelCount", [](vdb_basegrid_ptr_t grid) -> uint64_t {
      return grid->activeVoxelCount();
    });
  type_codec->registerStdCodec<vdb_basegrid_ptr_t>(grid_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct citer_proxy {
    openvdb::FloatGrid::ValueOnCIter iter;
    citer_proxy(openvdb::FloatGrid::ValueOnCIter iter) : iter(iter) {}
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
  auto ovdb_fgrid_iter_type = py::class_<citer_proxy,citer_proxy_ptr>(ovdb, "FloatGridIter")
    .def("__iter__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { return iter; })
    .def("__next__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { //
      auto next = std::make_shared<citer_proxy>(iter->iter);
      return next;
    });
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_fgrid_type = 
    py::class_<vdb_floatgrid_t, vdb_basegrid_t, vdb_floatgrid_ptr_t>(ovdb, "FloatGrid")
    .def_static("createLevelSetSphere", []( std::string name, float radius, fvec3 center, float vxlsize, float hwidth ) -> vdb_floatgrid_ptr_t {
      py::gil_scoped_release release;
      auto grid = openvdb::tools::createLevelSetSphere<vdb_floatgrid_t>(radius, openvdb::Vec3f(center.x,center.y,center.z), vxlsize, hwidth);
      grid->setName(name);
      //createLevelSetSphere (float radius, const openvdb::Vec3f &center, float voxelSize, float halfWidth=float(LEVEL_SET_HALF_WIDTH), InterruptT *interrupt=nullptr, bool threaded=true)
      return grid;
    })
    ///////////////////////////////////////////////////////
    .def_property("background", [](vdb_floatgrid_ptr_t grid ) -> float {
      return grid->background();
    }, [](vdb_floatgrid_ptr_t grid, float value) {
      size_t grainSize = 32;
      openvdb::tools::changeLevelSetBackground (grid->tree(), value, true, grainSize);
    })
    ///////////////////////////////////////////////////////
    .def_property_readonly("onValueSequence", [](vdb_floatgrid_ptr_t grid ) -> citer_proxy_ptr {
      auto iter = std::make_shared<citer_proxy>(grid->cbeginValueOn());
      return iter;
    })
    ///////////////////////////////////////////////////////
    .def("scatterVoxels", [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
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
          int itx = uniform_dist(e1)-1;
          int ity = uniform_dist(e1)-1;
          int itz = uniform_dist(e1)-1;
          icoord = icoord + openvdb::Coord(itx,ity,itz);
          result->tree().setValueOn(icoord, value);
        }
      }
      return result;
    })
    ///////////////////////////////////////////////////////
    .def("scatterVoxels2", [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
      py::gil_scoped_release release;
      auto result = grid->copyWithNewTree();
      static auto cq = std::make_shared<cq_t>();
      int num_points   = grid->tree().activeLeafVoxelCount();
      std::atomic<int> count = num_points;
      std::random_device rand_dev;

      for (auto itl0 = grid->tree().cbeginRootChildren(); itl0; ++itl0) {
        auto op = [=,&rand_dev]() {
          std::default_random_engine e1(rand_dev());
          std::uniform_int_distribution<int> uniform_dist(0, 2);
          FloatVoxel fv;
          for (auto itl1 = itl0->cbeginChildOn(); itl1; ++itl1) {
            for (auto itl2 = itl1->cbeginChildOn(); itl2; ++itl2) {
              const float* src_data = itl2->buffer().data();
              for (auto it_vox = itl2->cbeginValueOn(); it_vox; ++it_vox) {
                int itx = uniform_dist(e1)-1;
                int ity = uniform_dist(e1)-1;
                int itz = uniform_dist(e1)-1;
                fv.coord = it_vox.getCoord() + openvdb::Coord(itx,ity,itz);
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
      while((count.load()>0)){
        if(cq->try_pop(fv)){
          result->tree().setValueOn(fv.coord, fv.value);
          count--;
        }
        else{
          sched_yield();
        }
      }
      return result;
    })
    ///////////////////////////////////////////////////////
    .def("translatedVoxels", [](vdb_floatgrid_ptr_t grid, fvec3 trans) -> vdb_floatgrid_ptr_t {
      py::gil_scoped_release release;
      auto result = grid->copyWithNewTree();
      for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
        const auto& leaf = *leafIter;
        for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
          float value = *voxelIter;
          auto icoord = voxelIter.getCoord();
          icoord = icoord + openvdb::Coord(trans.x,trans.y,trans.z);
          result->tree().setValueOn(icoord, value);
        }
      }
      return result;
    })
    ///////////////////////////////////////////////////////
    .def("setVoxel", [](vdb_floatgrid_ptr_t grid, fvec3 coord, float value) {
      py::gil_scoped_release release;
      grid->tree().setValue(openvdb::Coord(coord.x,coord.y,coord.z), value);
    })
    ///////////////////////////////////////////////////////
    .def("worldToIndex", [](vdb_floatgrid_ptr_t grid, fvec3 coord) -> fvec3 {
      py::gil_scoped_release release;
      auto index = grid->transform().worldToIndex(openvdb::Vec3f(coord.x,coord.y,coord.z));
      return fvec3(index.x(),index.y(),index.z());
    })
    ///////////////////////////////////////////////////////
    .def("drawLineI", [](vdb_floatgrid_ptr_t grid, fvec3 start, fvec3 end, float value) {
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
        grid->tree().setValue(openvdb::Coord(ix,iy,iz), value);
        x += Xinc;
        y += Yinc;
        z += Zinc;
      }
    })
    ///////////////////////////////////////////////////////
    .def("toQuads", [](vdb_floatgrid_ptr_t grid, float isovalue) -> py::dict {
      std::vector< openvdb::Vec3s > points;
      std::vector< openvdb::Vec4I > quads;
      {
          py::gil_scoped_release release;
          openvdb::tools::volumeToMesh(*grid, points, quads,isovalue);
      }
      auto vertices = py::list();
      auto indices = py::list();
      for (auto& point : points) {
        auto world = grid->transform().indexToWorld(point);
        vertices.append(fvec3(point.x(),point.y(),point.z()));
      }
      for (auto& quad : quads) {
        indices.append(4);
        indices.append(quad[3]);
        indices.append(quad[2]);
        indices.append(quad[1]);
        indices.append(quad[0]);
      }
      auto result = py::dict();
      result["vertices"] = vertices;
      result["faces"] = indices;
      return result;
    });
  type_codec->registerStdCodec<vdb_floatgrid_ptr_t>(ovdb_fgrid_type);
  /////////////////////////////////////////////////////////////////////////////////
  // AX volume executable
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_ax_ve_type = 
    py::class_<vdb_volume_exec_t, vdb_volume_exec_ptr_t>(ax, "VolumeExecutable")
    .def_static("compile", [](std::string code, vdb_custom_data_ptr_t cdata=nullptr) -> vdb_volume_exec_ptr_t {
      py::gil_scoped_release release;
      openvdb::ax::Compiler compiler;
      return compiler.compile<vdb_volume_exec_t>(code,cdata);
    })
    .def("executeOnGrid", [](vdb_volume_exec_ptr_t ve, vdb_floatgrid_ptr_t grid) {
      py::gil_scoped_release release;
      ve->execute(*grid);
    });
    type_codec->registerStdCodec<vdb_volume_exec_ptr_t>(ovdb_ax_ve_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cdata_type = 
    py::class_<vdb_custom_data_t, vdb_custom_data_ptr_t>(ax, "CustomData")
    .def(py::init<>())
    .def("set", [](vdb_custom_data_ptr_t cdata, std::string key, py::object value) {
      if (py::isinstance<py::int_>(value)) {
        auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<int>>(key);
        typed->setValue(py::cast<int>(value));
      } else if (py::isinstance<py::float_>(value)) {
        auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<float>>(key);
        //printf("set float key<%s> value<%f>\n", key.c_str(), py::cast<float>(value));
        typed->setValue(py::cast<float>(value));
      } else {
        OrkAssert(false); // unsupported type
      }
    });
    type_codec->registerStdCodec<vdb_custom_data_ptr_t>(cdata_type);

} 
} //namespace ork::lev2 {
