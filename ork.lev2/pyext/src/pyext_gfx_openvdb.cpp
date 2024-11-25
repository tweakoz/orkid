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
#include <openvdb/util/NullInterrupter.h>
#include <ork/python/obind/nanobind.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

using vdb_basegrid_t       = openvdb::GridBase;
using vdb_basegrid_ptr_t   = std::shared_ptr<vdb_basegrid_t>;
using vdb_floatgrid_t       = openvdb::FloatGrid;
using vdb_floatgrid_ptr_t   = std::shared_ptr<vdb_floatgrid_t>;


void pyinit_gfx_openvdb(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb = module_lev2.def_submodule("vdb", "OrkidOpenVDBBridge");

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
    .def_static("createLevelSetSphere", [](float radius, fvec3 center, float vxlsize, float hwidth ) -> vdb_floatgrid_ptr_t {
      auto grid = openvdb::tools::createLevelSetSphere<vdb_floatgrid_t>(radius, openvdb::Vec3f(center.x,center.y,center.z), vxlsize, hwidth);
      //createLevelSetSphere (float radius, const openvdb::Vec3f &center, float voxelSize, float halfWidth=float(LEVEL_SET_HALF_WIDTH), InterruptT *interrupt=nullptr, bool threaded=true)
      return grid;
    })
    .def_property("background", [](vdb_floatgrid_ptr_t grid ) -> float {
      return grid->background();
    }, [](vdb_floatgrid_ptr_t grid, float value) {
      size_t grainSize = 32;
      openvdb::tools::changeLevelSetBackground (grid->tree(), value, true, grainSize);
    })
    .def_property_readonly("onValueSequence", [](vdb_floatgrid_ptr_t grid ) -> citer_proxy_ptr {
      auto iter = std::make_shared<citer_proxy>(grid->cbeginValueOn());
      return iter;
    });
  type_codec->registerStdCodec<vdb_floatgrid_ptr_t>(ovdb_fgrid_type);

}

} //namespace ork::lev2 {
