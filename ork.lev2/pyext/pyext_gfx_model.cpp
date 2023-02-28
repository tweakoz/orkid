////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_xgmmodel(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmModel, xgmmodel_ptr_t>(module_lev2, "XgmModel") //
      .def(py::init([](const std::string& model_path) -> xgmmodel_ptr_t {
        auto loadreq    = std::make_shared<asset::LoadRequest>(model_path.c_str());
        auto modl_asset = asset::AssetManager<XgmModelAsset>::load(loadreq);
        return modl_asset->_model.atomicCopy();
      }))
      .def_property_readonly(
          "boundingCenter", //
          [](xgmmodel_ptr_t model) -> fvec3 { //
            return model->boundingCenter();
          })
      .def_property_readonly(
          "boundingRadius", //
          [](xgmmodel_ptr_t model) -> float { //
            return model->GetBoundingRadius();
          })
      .def(
          "createNode",         //
          [](xgmmodel_ptr_t model, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::node_ptr_t { //
            auto drw        = std::make_shared<ModelDrawable>(nullptr);
            drw->_modelinst = std::make_shared<XgmModelInst>(model.get());

            auto node = layer->createDrawableNode(named, drw);
            node->_userdata->makeValueForKey<xgmmodel_ptr_t>("pyext.retain.model", model);
            return node;
          })
      .def(
          "createInstancedNode", //
          [](xgmmodel_ptr_t model,  //
             int numinstances,
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::drawable_node_ptr_t { //
            auto drw = std::make_shared<InstancedModelDrawable>();
            drw->bindModel(model);
            auto node = layer->createDrawableNode(named, drw);
            drw->resize(numinstances);
            auto instdata = drw->_instancedata;
            for (int i = 0; i < numinstances; i++) {
              instdata->_worldmatrices[i].compose(fvec3(0, 0, 0), fquat(), 0.0f);
            }
            return node;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmMesh, xgmmesh_ptr_t>(module_lev2, "XgmMesh");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmSubMesh, xgmsubmesh_ptr_t>(module_lev2, "XgmSubMesh")
      .def_property_readonly(
          "clusters", //
          [](xgmsubmesh_ptr_t submesh) -> py::list { //
            auto pyl = py::list();
            for( auto item : submesh->_clusters ){
              pyl.append(item);
            }
            return pyl;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmCluster, xgmcluster_ptr_t>(module_lev2, "XgmCluster");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmPrimGroup, xgmprimgroup_ptr_t>(module_lev2, "XgmPrimGroup");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmSubMeshInst, xgmsubmeshinst_ptr_t>(module_lev2, "XgmSubMeshInst");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmModelInst, xgmmodelinst_ptr_t>(module_lev2, "XgmModelInst");

    }

  } //  namespace ork::lev2 {

