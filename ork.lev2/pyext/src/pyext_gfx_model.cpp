////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_xgmmodel(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto model_type_t = py::class_<XgmModel, xgmmodel_ptr_t>(module_lev2, "XgmModel") //
      .def(py::init([](const std::string& model_path) -> xgmmodel_ptr_t {
        auto loadreq    = std::make_shared<asset::LoadRequest>(model_path.c_str());
        auto modl_asset = asset::AssetManager<XgmModelAsset>::load(loadreq);
        return modl_asset->_model.atomicCopy();
      }))
      .def_property_readonly(
          "skeleton", //
          [](xgmmodel_ptr_t model) -> xgmskeleton_ptr_t { //
            return model->_skeleton;
          })
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
      .def_property_readonly(
          "meshes", //
          [](xgmmodel_ptr_t model) -> py::list { //
            auto pyl = py::list();
            for( auto item : model->mMeshes ){
              pyl.append(item.second);
            }
            return pyl;
          })
      .def_property_readonly(
          "materials", //
          [](xgmmodel_ptr_t model) -> py::list { //
            auto pyl = py::list();
            for( auto item : model->mvMaterials ){
              pyl.append(item);
            }
            return pyl;
          })
      .def(
          "createDrawable",         //
          [](xgmmodel_ptr_t model) -> drawable_ptr_t { //
            auto drw        = std::make_shared<ModelDrawable>(nullptr);
            drw->_modelinst = std::make_shared<XgmModelInst>(model.get());
            return drw;
          })
      .def(
          "createNode",         //
          [](xgmmodel_ptr_t model, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::node_ptr_t { //
            auto drw        = std::make_shared<ModelDrawable>(nullptr);
            drw->_modelinst = std::make_shared<XgmModelInst>(model.get());

            auto node = layer->createDrawableNode(named, drw);
            node->_userdata->makeValueForKey<xgmmodel_ptr_t>("pyext_retain_model", model);
            node->_userdata->makeValueForKey<xgmmodelinst_ptr_t>("pyext_retain_modelinst", drw->_modelinst);
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
          })
        .def(
          "mAABoundXYZ",
          [](xgmmodel_ptr_t model) -> fvec3
          {
            return model->mAABoundXYZ;
          }
        )
        .def(
          "mAABoundWHD",
          [](xgmmodel_ptr_t model) -> fvec3
          {
            return model->mAABoundWHD;
          }
        )
        .def(
          "IntersectBoundingBox",
          [](xgmmodel_ptr_t model,
          const fray3 &ray, 
          fvec3 &isect_in, 
          fvec3 &isect_out
          ) -> bool
          {
            return model->IntersectBoundingBox(ray, isect_in, isect_out);
          }
        )
        ;
  type_codec->registerStdCodec<xgmmodel_ptr_t>(model_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto mesh_type_t = py::class_<XgmMesh, xgmmesh_ptr_t>(module_lev2, "XgmMesh")
      .def_property_readonly(
          "name", //
          [](xgmmesh_ptr_t mesh) -> std::string { //
            return mesh->mMeshName.c_str();
          })
      .def_property_readonly(
          "boundingRadius", //
          [](xgmmesh_ptr_t mesh) -> float { //
            return mesh->mfBoundingRadius;
          })
      .def_property_readonly(
          "boundingCenter", //
          [](xgmmesh_ptr_t mesh) -> fvec3 { //
            return mesh->mvBoundingCenter.xyz();
          })
      .def_property_readonly(
          "submeshes", //
          [](xgmmesh_ptr_t mesh) -> py::list { //
            auto pyl = py::list();
            for( auto item : mesh->mSubMeshes ){
              pyl.append(item);
            }
            return pyl;
          });
  type_codec->registerStdCodec<xgmmesh_ptr_t>(mesh_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto submesh_type_t = py::class_<XgmSubMesh, xgmsubmesh_ptr_t>(module_lev2, "XgmSubMesh")
      .def_property_readonly(
          "clusters", //
          [](xgmsubmesh_ptr_t submesh) -> py::list { //
            auto pyl = py::list();
            for( auto item : submesh->_clusters ){
              pyl.append(item);
            }
            return pyl;
          })
      .def_property(
          "material", //
          [](xgmsubmesh_ptr_t submesh) -> material_ptr_t { //
            return submesh->_material;
          },
          [](xgmsubmesh_ptr_t submesh, material_ptr_t m) { //
            submesh->_material = m;
          });
  type_codec->registerStdCodec<xgmsubmesh_ptr_t>(submesh_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmCluster, xgmcluster_ptr_t>(module_lev2, "XgmCluster");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmPrimGroup, xgmprimgroup_ptr_t>(module_lev2, "XgmPrimGroup");
  /////////////////////////////////////////////////////////////////////////////////
  auto submeshinst_type_t = py::class_<XgmSubMeshInst, xgmsubmeshinst_ptr_t>(module_lev2, "XgmSubMeshInst")
        .def_property_readonly(
          "material", [](xgmsubmeshinst_ptr_t smi) -> material_ptr_t {
            return smi->material();
          })
      .def("overrideMaterial", //
          [](xgmsubmeshinst_ptr_t submeshinst,material_ptr_t m) { //
            submeshinst->overrideMaterial(m);
          });
  type_codec->registerStdCodec<xgmsubmeshinst_ptr_t>(submeshinst_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto modelinst_type_t = py::class_<XgmModelInst, xgmmodelinst_ptr_t>(module_lev2, "XgmModelInst")
      .def(py::init([](xgmmodel_ptr_t model) -> xgmmodelinst_ptr_t {
        return std::make_shared<XgmModelInst>(model.get());
      }))
      .def("enableSkinning", //
          [](xgmmodelinst_ptr_t minst) { //
            minst->enableSkinning();
          })
      .def("enableAllMeshes", //
          [](xgmmodelinst_ptr_t minst) { //
            minst->enableAllMeshes();
          })
        .def_property_readonly(
          "submeshinsts", //
          [](xgmmodelinst_ptr_t minst) -> py::list { //
            auto pyl = py::list();
            for( auto item : minst->_submeshinsts ){
              pyl.append(item);
            }
            return pyl;
          })
        .def_property_readonly(
          "localpose", //
          [](xgmmodelinst_ptr_t minst) -> xgmlocalpose_ptr_t { //
            return minst->_localPose;
          })
        .def_property_readonly(
          "worldpose", //
          [](xgmmodelinst_ptr_t minst) -> xgmworldpose_ptr_t { //
            return minst->_worldPose;
          })
        .def_property(
          "drawSkeleton", //
          [](xgmmodelinst_ptr_t minst) -> bool { //
            return minst->_drawSkeleton;
          },
          [](xgmmodelinst_ptr_t minst, bool bv) { //
            minst->_drawSkeleton = bv;
          });
  type_codec->registerStdCodec<xgmmodelinst_ptr_t>(modelinst_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////

    }

  } //  namespace ork::lev2 {

