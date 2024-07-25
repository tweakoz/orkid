////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/ikchain.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_xgmanim(py::module& module_lev2) {
  auto type_codec                  = python::pb11_typecodec_t::instance();
  module_lev2.attr("animMaxBones") = kmaxbones;
  /////////////////////////////////////////////////////////////////////////////////
  auto dcmtx_type_t = py::class_<DecompMatrix>(module_lev2, "DecompMatrix") //
      .def(py::init([]() -> DecompMatrix {
        return DecompMatrix();
      }))
      .def_property(
          "orientation", //
          [](DecompMatrix& self) -> fquat { //
            return self._orientation;
          },
          [](DecompMatrix& self, const fquat& val) { //
            self._orientation = val;
          })
      .def_property(
          "scale", //
          [](DecompMatrix& self) -> fvec3 { //
            return self._scale;
          },
          [](DecompMatrix& self, const fvec3& val) { //
            self._scale = val;
          })
      .def_property(
          "translation", //
          [](DecompMatrix& self) -> fvec3 { //
            return self._position;
          },
          [](DecompMatrix& self, const fvec3& val) { //
            self._position = val;
          })
      .def("__repr__", [](const DecompMatrix& self) -> std::string {
        auto oristr = FormatString("%g %g %g %g", self._orientation.x, self._orientation.y, self._orientation.z, self._orientation.w);
        auto sclstr = FormatString("%g %g %g", self._scale.x, self._scale.y, self._scale.z);
        auto posstr = FormatString("%g %g %g", self._position.x, self._position.y, self._position.z);
        return FormatString("DecompMatrix: ori[%s] scl[%s] tra[%s]", //
                            oristr.c_str(), sclstr.c_str(), posstr.c_str());
      });
  type_codec->registerStdCodec<DecompMatrix>(dcmtx_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto anim_type_t = py::class_<XgmAnim, xgmanim_ptr_t>(module_lev2, "XgmAnim") //
                         .def(py::init([](const std::string& anim_path) -> xgmanim_ptr_t {
                           auto loadreq    = std::make_shared<asset::LoadRequest>(anim_path.c_str());
                           auto anim_asset = asset::AssetManager<XgmAnimAsset>::load(loadreq);
                           return anim_asset->_animation;
                         }))
                         .def_property_readonly(
                             "numJointChannels",                //
                             [](xgmanim_ptr_t anim) -> size_t { //
                               return anim->_jointanimationchannels.size();
                             })
                         .def_property_readonly(
                             "numMaterialChannels",             //
                             [](xgmanim_ptr_t anim) -> size_t { //
                               return anim->mMaterialAnimationChannels.size();
                             });
  type_codec->registerStdCodec<xgmanim_ptr_t>(anim_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animinst_type_t = py::class_<XgmAnimInst, xgmaniminst_ptr_t>(module_lev2, "XgmAnimInst") //
                             .def(py::init([]() -> xgmaniminst_ptr_t { return std::make_shared<XgmAnimInst>(); }))
                             .def(py::init([](xgmanim_constptr_t anim) -> xgmaniminst_ptr_t {
                               auto rval = std::make_shared<XgmAnimInst>();
                               rval->bindAnim(anim);
                               return rval;
                             }))
                             .def(
                                 "bindAnim",                                           //
                                 [](xgmaniminst_ptr_t self, xgmanim_constptr_t anim) { //
                                   self->bindAnim(anim);
                                 })
                             .def(
                                 "bindToSkeleton",                                    //
                                 [](xgmaniminst_ptr_t self, xgmskeleton_ptr_t skel) { //
                                   self->bindToSkeleton(skel);
                                 })
                             .def(
                                 "applyToPose",                                           //
                                 [](xgmaniminst_ptr_t self, xgmlocalpose_ptr_t localpose) { //
                                   self->applyToPose(localpose);
                                 })
                             .def_property_readonly(
                                 "mask",                                           //
                                 [](xgmaniminst_ptr_t self) -> xgmanimmask_ptr_t { //
                                   return self->_mask;
                                 })
                             .def_property_readonly(
                                 "poser",                                       //
                                 [](xgmaniminst_ptr_t self) -> xgmposer_ptr_t { //
                                   return self->_poser;
                                 })
                             .def_property_readonly(
                                 "sampleRate",                         //
                                 [](xgmaniminst_ptr_t self) -> float { //
                                   return self->GetSampleRate();
                                 })
                             .def_property_readonly(
                                 "numFrames",                           //
                                 [](xgmaniminst_ptr_t self) -> size_t { //
                                   return self->numFrames();
                                 })
                             .def_property(
                                 "weight",                             //
                                 [](xgmaniminst_ptr_t self) -> float { //
                                   return self->GetWeight();
                                 },
                                  [](xgmaniminst_ptr_t self, float fv) { //
                                    self->SetWeight(fv);
                                  })
                             .def_property(
                                 "use_temporal_lerp",                 //
                                 [](xgmaniminst_ptr_t self) -> bool { //
                                   return self->_use_temporal_lerp;
                                 },
                                 [](xgmaniminst_ptr_t self, bool bv) { //
                                   self->_use_temporal_lerp = bv;
                                 })
                             .def_property(
                                 "currentFrame",                 //
                                 [](xgmaniminst_ptr_t self) -> float { //
                                   return self->_current_frame;
                                 },
                                 [](xgmaniminst_ptr_t self, float bv) { //
                                   self->_current_frame = fmod(bv,self->numFrames());
                                 });

  type_codec->registerStdCodec<xgmaniminst_ptr_t>(animinst_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animmask_type_t = py::class_<XgmAnimMask, xgmanimmask_ptr_t>(module_lev2, "XgmAnimMask") //
                             .def_property_readonly(
                                 "bytes",
                                 [](xgmanimmask_ptr_t self) -> std::string {
                                   std::string rval;
                                   for (int i = 0; i < XgmAnimMask::knummaskbytes; i++) {
                                     rval += FormatString("%02x", self->mMaskBits[i]);
                                     if (i != (XgmAnimMask::knummaskbytes - 1))
                                       rval += ":";
                                   }
                                   return rval;
                                 })
                             .def("enableAll", [](xgmanimmask_ptr_t self) { self->EnableAll(); })
                             .def("disableAll", [](xgmanimmask_ptr_t self) { self->DisableAll(); })
                             .def(
                                 "enableBone",
                                 [](xgmanimmask_ptr_t self, xgmskeleton_constptr_t skeleton, const std::string& namedBone) {
                                   self->Enable(skeleton, namedBone);
                                 })
                             .def(
                                 "disableBone",
                                 [](xgmanimmask_ptr_t self, xgmskeleton_constptr_t skeleton, const std::string& namedBone) {
                                   self->Disable(skeleton, namedBone);
                                 })
                             .def("enableBoneIndex", [](xgmanimmask_ptr_t self, int bone_index) { self->Enable(bone_index); })
                             .def("disableBoneIndex", [](xgmanimmask_ptr_t self, int bone_index) { self->Disable(bone_index); });
  type_codec->registerStdCodec<xgmanimmask_ptr_t>(animmask_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animposer_type_t = py::class_<XgmPoser, xgmposer_ptr_t>(module_lev2, "XgmPoser")
                              .def(
                                  "poseBinding",
                                  [](xgmposer_ptr_t self, int index) -> XgmSkeletonBinding& {
                                    OrkAssert(index < kmaxbones);
                                    return self->_poseBindings[index];
                                  })
                              .def("animBinding", [](xgmposer_ptr_t self, int index) -> XgmSkeletonBinding& {
                                OrkAssert(index < kmaxbones);
                                return self->_animBindings[index];
                              });
  type_codec->registerStdCodec<xgmposer_ptr_t>(animposer_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animskelbinding_type_t = py::class_<XgmSkeletonBinding>(module_lev2, "XgmSkeletonBinding")
                                    .def_property(
                                        "skeletonIndex",
                                        [](const XgmSkeletonBinding& self) -> int { return self.mSkelIndex; },
                                        [](XgmSkeletonBinding& self, int idx) { self.mSkelIndex = idx; })
                                    .def_property(
                                        "channelIndex",
                                        [](const XgmSkeletonBinding& self) -> int { return self.mChanIndex; },
                                        [](XgmSkeletonBinding& self, int idx) { self.mChanIndex = idx; });
  // type_codec->registerStdCodec<XgmSkeletonBinding>(animskelbinding_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto bone_type_t = py::class_<XgmBone>(module_lev2, "XgmBone")
    .def_property_readonly(
        "parentIndex",                                     //
        [](const XgmBone& self) -> int { //
          return self._parentIndex;
        })
    .def_property_readonly(
        "childIndex",                                     //
        [](const XgmBone& self) -> int { //
          return self._childIndex;
        });
  type_codec->registerStdCodec<XgmBone>(bone_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animskel_type_t = py::class_<XgmSkeleton, xgmskeleton_ptr_t>(module_lev2, "XgmSkeleton")
                             .def_property_readonly(
                                 "name",                                     //
                                 [](xgmskeleton_ptr_t self) -> std::string { //
                                   return self->msSkelName;
                                 })
                             .def_property_readonly(
                                 "numJoints",                        //
                                 [](xgmskeleton_ptr_t self) -> int { //
                                   return self->numJoints();
                                 })
                             .def_property_readonly(
                                 "numBones",                         //
                                 [](xgmskeleton_ptr_t self) -> int { //
                                   return self->numBones();
                                 })
                             .def(
                                 "jointName",                                           //
                                 [](xgmskeleton_ptr_t self, int index) -> std::string { //
                                   return self->_jointNAMES[index];
                                 })
                             .def(
                                 "jointPath",                                           //
                                 [](xgmskeleton_ptr_t self, int index) -> std::string { //
                                   return self->_jointPATHS[index];
                                 })
                             .def(
                                 "jointID",                                           //
                                 [](xgmskeleton_ptr_t self, int index) -> std::string { //
                                   return self->_jointIDS[index];
                                 })
                             .def(
                                 "jointIndex",                                           //
                                 [](xgmskeleton_ptr_t self, std::string named) -> int { //
                                   return self->jointIndex(named);
                                 })
                             .def(
                                 "selectBone",                                           //
                                 [](xgmskeleton_ptr_t self, int index) { //
                                   return self->selectBoneIndex(index);
                                 })
                             .def(
                                 "bone",                                   //
                                 [](xgmskeleton_ptr_t self, int index) -> XgmBone { //
                                   return self->_bones[index];
                                 })
                             .def(
                                 "jointParent",                                   //
                                 [](xgmskeleton_ptr_t self, int index) -> int { //
                                   return self->_parentIndices[index];
                                 })
                             .def(
                                 "jointMatrix",                                   //
                                 [](xgmskeleton_ptr_t self, int index) -> fmtx4 { //
                                   return self->RefNodeMatrix(index);
                                 })
                             .def(
                                 "bindMatrix",                                    //
                                 [](xgmskeleton_ptr_t self, int index) -> fmtx4 { //
                                   return self->_bindMatrices[index];
                                 })
                             .def_property_readonly("bindMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_bindMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly("inverseBindMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_inverseBindMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def(
                                 "descendantJointsOf",                                    //
                                 [](xgmskeleton_ptr_t self, int index) -> py::list { //
                                   auto children = self->descendantJointsOf(index);
                                   py::list rval;
                                    for (auto c : children) {
                                      rval.append(c);
                                    }
                                   return rval;
                                 })
                             .def(
                                 "childJointsOf",                                    //
                                 [](xgmskeleton_ptr_t self, int index) -> py::list { //
                                   auto children = self->childJointsOf(index);
                                   py::list rval;
                                    for (auto c : children) {
                                      rval.append(c);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "jointMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_jointMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "nodeMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_nodeMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "bindMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_bindMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "inverseBindMatrices",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      rval.append(self->_inverseBindMatrices[i]);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "jointVertexInfluenceCounts",                                    //
                                 [](xgmskeleton_ptr_t self) -> py::list { //
                                   py::list rval;
                                    for (int i = 0; i < self->numJoints(); i++) {
                                      auto jp = self->_jointProperties[i];
                                      rval.append(jp->_numVerticesInfluenced);
                                    }
                                   return rval;
                                 })
                             .def_property_readonly(
                                 "rootNodeIndex",                                    //
                                 [](xgmskeleton_ptr_t self) -> int { //
                                   return self->miRootNode;
                                 })
                             .def_property(
                                 "visualBoneScale",                                    //
                                 [](xgmskeleton_ptr_t self) -> float { //
                                   return self->_visbonescale;
                                 },
                                  [](xgmskeleton_ptr_t self, float f) { //
                                    self->_visbonescale = f;
                                  });
  type_codec->registerStdCodec<xgmskeleton_ptr_t>(animskel_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  struct LocalMatrixInterface {
      LocalMatrixInterface(xgmlocalpose_ptr_t p) : pose(p) {}
      fmtx4 get(int index) {
          return pose->_local_matrices[index];
      }
      void set(int index, const fmtx4 &value) {
          pose->_local_matrices[index] = value;
      }
      xgmlocalpose_ptr_t pose;
  };
  struct ConcatMatrixInterface {
      ConcatMatrixInterface(xgmlocalpose_ptr_t p) : pose(p) {}
      fmtx4 get(int index) {
          return pose->_concat_matrices[index];
      }
      void set(int index, const fmtx4 &value) {
          pose->_concat_matrices[index] = value;
      }
      xgmlocalpose_ptr_t pose;
  };
  struct BindRelaMatrixInterface {
      BindRelaMatrixInterface(xgmlocalpose_ptr_t p) : pose(p) {}
      fmtx4 get(int index) {
          return pose->_bindrela_matrices[index];
      }
      void set(int index, const fmtx4 &value) {
          pose->_bindrela_matrices[index] = value;
      }
      xgmlocalpose_ptr_t pose;
  };
  py::class_<LocalMatrixInterface>(module_lev2, "XgmLocalPoseLocalMatrixInterface")
      .def("__getitem__", &LocalMatrixInterface::get)
      .def("__getitem__", [](LocalMatrixInterface& LMI, py::slice slicer) -> py::list {
        ssize_t start, stop, step, slicelength;
        if (!slicer.compute(LMI.pose->NumJoints(), &start, &stop, &step, &slicelength)){
          OrkAssert(false);
        }
        py::list rval;
        for (int i = start; i < stop; i += step) {
          rval.append(LMI.get(i));
        }
        return rval;
      })
      .def("__setitem__", &LocalMatrixInterface::set)
      .def_property_readonly("as_list", [](LocalMatrixInterface& LMI) -> py::list {
        py::list rval;
        for (int i = 0; i < LMI.pose->NumJoints(); i++) {
          auto as_py = py::cast(LMI.get(i));
          rval.append(as_py);
        }
        return rval;
      });
  py::class_<ConcatMatrixInterface>(module_lev2, "XgmLocalPoseConcatMatrixInterface")
      .def("__getitem__", &ConcatMatrixInterface::get)
      .def("__getitem__", [](ConcatMatrixInterface& CMI, py::slice slicer) -> py::list {
        ssize_t start, stop, step, slicelength;
        if (!slicer.compute(CMI.pose->NumJoints(), &start, &stop, &step, &slicelength)){
          OrkAssert(false);
        }
        py::list rval;
        for (int i = start; i < stop; i += step) {
          rval.append(CMI.get(i));
        }
        return rval;
      })
      .def("__setitem__", &ConcatMatrixInterface::set)
      .def_property_readonly("as_list", [](ConcatMatrixInterface& CMI) -> py::list {
        py::list rval;
        for (int i = 0; i < CMI.pose->NumJoints(); i++) {
          auto as_py = py::cast(CMI.get(i));
          rval.append(as_py);
        }
        return rval;
      });
  py::class_<BindRelaMatrixInterface>(module_lev2, "XgmLocalPoseBindRelaMatrixInterface")
      .def("__getitem__", &BindRelaMatrixInterface::get)
      .def("__getitem__", [](BindRelaMatrixInterface& BMI, py::slice slicer) -> py::list {
        ssize_t start, stop, step, slicelength;
        if (!slicer.compute(BMI.pose->NumJoints(), &start, &stop, &step, &slicelength)){
          OrkAssert(false);
        }
        py::list rval;
        for (int i = start; i < stop; i += step) {
          rval.append(BMI.get(i));
        }
        return rval;
      })
      .def("__setitem__", &BindRelaMatrixInterface::set)
      .def_property_readonly("as_list", [](BindRelaMatrixInterface& BMI) -> py::list {
        py::list rval;
        for (int i = 0; i < BMI.pose->NumJoints(); i++) {
          auto as_py = py::cast(BMI.get(i));
          rval.append(as_py);
        }
        return rval;
      });
  ///
  auto lpose_type_t =
      py::class_<XgmLocalPose, xgmlocalpose_ptr_t>(module_lev2, "XgmLocalPose")
          .def(py::init([](xgmskeleton_ptr_t skel) -> xgmlocalpose_ptr_t { return std::make_shared<XgmLocalPose>(skel); }))
          .def("identityPose", [](xgmlocalpose_ptr_t self) { return self->identityPose(); })
          .def("bindPose", [](xgmlocalpose_ptr_t self) { return self->bindPose(); })
          .def("blendPoses", [](xgmlocalpose_ptr_t self) { return self->blendPoses(); })
          .def("concatenate", [](xgmlocalpose_ptr_t self) { return self->concatenate(); })
          .def("deconcatenate", [](xgmlocalpose_ptr_t self) { return self->decomposeConcatenated(); })
          .def("poseJoint", [](xgmlocalpose_ptr_t self, int index, float fweight, DecompMatrix& mtx) { //
            self->poseJoint(index,fweight,mtx);
           })
          .def("transformOnPostConcat", [](xgmlocalpose_ptr_t self, int index, const fmtx4& mtx) { //
            self->transformOnPostConcat(index,mtx);
           })
          .def("decompLocal", [](xgmlocalpose_ptr_t self, int index) -> DecompMatrix  { //
            return self->decompLocal(index);
           })
          .def_property_readonly("localMatrices", [](xgmlocalpose_ptr_t self) {
            return LocalMatrixInterface(self);
          })
          .def_property_readonly("concatMatrices", [](xgmlocalpose_ptr_t self) {
            return ConcatMatrixInterface(self);
          })
          .def_property_readonly("bindRelativeMatrices", [](xgmlocalpose_ptr_t self) {
            return BindRelaMatrixInterface(self);
          })
          .def_property_readonly("numJoints", [](xgmlocalpose_ptr_t self) -> int { return self->NumJoints(); })
          .def_property_readonly(
              "objSpaceBoundingSphere", [](xgmlocalpose_ptr_t self) -> fvec4 { return self->mObjSpaceBoundingSphere; });

  type_codec->registerStdCodec<xgmlocalpose_ptr_t>(lpose_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  struct WorldPoseConcatMatrixInterface {
      WorldPoseConcatMatrixInterface(xgmworldpose_ptr_t p) : pose(p) {}
      fmtx4 get(int index) {
          return pose->_world_concat_matrices[index];
      }
      void set(int index, const fmtx4 &value) {
          pose->_world_concat_matrices[index] = value;
      }
      xgmworldpose_ptr_t pose;
  };
  struct WorldPoseBindRelaMatrixInterface {
      WorldPoseBindRelaMatrixInterface(xgmworldpose_ptr_t p) : pose(p) {}
      fmtx4 get(int index) {
          return pose->_world_bindrela_matrices[index];
      }
      void set(int index, const fmtx4 &value) {
          pose->_world_bindrela_matrices[index] = value;
      }
      xgmworldpose_ptr_t pose;
  };
  py::class_<WorldPoseConcatMatrixInterface>(module_lev2, "XgmWorldPoseConcatMatrixInterface")
      .def("__getitem__", &WorldPoseConcatMatrixInterface::get)
      .def("__getitem__", [](WorldPoseConcatMatrixInterface& CMI, py::slice slicer) -> py::list {
        ssize_t start, stop, step, slicelength;
        if (!slicer.compute(CMI.pose->_world_bindrela_matrices.size(), &start, &stop, &step, &slicelength)){
          OrkAssert(false);
        }
        py::list rval;
        for (int i = start; i < stop; i += step) {
          rval.append(CMI.get(i));
        }
        return rval;
      })
      .def("__setitem__", &WorldPoseConcatMatrixInterface::set);
  py::class_<WorldPoseBindRelaMatrixInterface>(module_lev2, "XgmLocalPoseWorldPoseBindRelaMatrixInterface")
      .def("__getitem__", &WorldPoseBindRelaMatrixInterface::get)
      .def("__getitem__", [](WorldPoseBindRelaMatrixInterface& BMI, py::slice slicer) -> py::list {
        ssize_t start, stop, step, slicelength;
        if (!slicer.compute(BMI.pose->_world_bindrela_matrices.size(), &start, &stop, &step, &slicelength)){
          OrkAssert(false);
        }
        py::list rval;
        for (int i = start; i < stop; i += step) {
          rval.append(BMI.get(i));
        }
        return rval;
      })
      .def("__setitem__", &WorldPoseBindRelaMatrixInterface::set);
  /////////////////////////////////////////////////////////////////////////////////
    auto wpose_type_t =
      py::class_<XgmWorldPose, xgmworldpose_ptr_t>(module_lev2, "XgmWorldPose")
          .def(py::init([](xgmskeleton_ptr_t skel) -> xgmworldpose_ptr_t { return std::make_shared<XgmWorldPose>(skel); }))
          .def("fromLocalPose", [](xgmworldpose_ptr_t self, xgmlocalpose_ptr_t lpose, fmtx4& world_matrix) {
            self->apply(world_matrix, lpose);
          })
          .def_property_readonly("concatMatrices", [](xgmworldpose_ptr_t self) {
            return WorldPoseConcatMatrixInterface(self);
          })
          .def_property_readonly("bindRelativeMatrices", [](xgmworldpose_ptr_t self) {
            return WorldPoseBindRelaMatrixInterface(self);
          });
  type_codec->registerStdCodec<xgmworldpose_ptr_t>(wpose_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto bxf_type_t =
      py::class_<BoneTransformer, bone_transformer_ptr_t>(module_lev2, "BoneTransformer")
          .def(py::init([](xgmskeleton_ptr_t skel) -> bone_transformer_ptr_t { return std::make_shared<BoneTransformer>(skel); }))
          .def(
              "bindToBone",                                        //
              [](bone_transformer_ptr_t self, std::string named) { //
                return self->bindToBone(named);
              })
          .def(
              "compute",                                        //
              [](bone_transformer_ptr_t self, xgmlocalpose_ptr_t localpose, fmtx4 matrix) { //
                return self->compute(localpose,matrix);
              });
  type_codec->registerStdCodec<bone_transformer_ptr_t>(bxf_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto ikc_type_t = py::class_<IkChain, ikchain_ptr_t>(module_lev2, "IkChain")
  .def(py::init([](xgmskeleton_ptr_t skel) -> ikchain_ptr_t { //
    return std::make_shared<IkChain>(skel);
  }))
  .def(
      "bindToJointNamed",                               //
      [](ikchain_ptr_t self, std::string named) { //
        return self->bindToJointNamed(named);
      })
  .def(
      "bindToJointPath",                               //
      [](ikchain_ptr_t self, std::string named) { //
        return self->bindToJointPath(named);
      })
  .def(
      "bindToJointID",                               //
      [](ikchain_ptr_t self, std::string named) { //
        return self->bindToJointID(named);
      })
  .def(
      "prepare",               //
      [](ikchain_ptr_t self) { //
        self->prepare();
      })
  .def(
      "compute", //
      [](ikchain_ptr_t self,
         xgmlocalpose_ptr_t localpose, //
         const fvec3& target) {        //
        self->compute(localpose, target);
      })
  .def_property(
      "C1",                 //
      [](ikchain_ptr_t self) -> float { //
        return self->_C1;
      },
      [](ikchain_ptr_t self, float val) { //
        self->_C1 = val;
      })
  .def_property(
      "C2",                 //
      [](ikchain_ptr_t self) -> float { //
        return self->_C2;
      },
      [](ikchain_ptr_t self, float val) { //
        self->_C2 = val;
      })
  .def_property(
      "C3",                 //
      [](ikchain_ptr_t self) -> float { //
        return self->_C3;
      },
      [](ikchain_ptr_t self, float val) { //
        self->_C3 = val;
      })
  .def_property(
      "C4",                 //
      [](ikchain_ptr_t self) -> float { //
        return self->_C4;
      },
      [](ikchain_ptr_t self, float val) { //
        self->_C4 = val;
      });
  type_codec->registerStdCodec<ikchain_ptr_t>(ikc_type_t);

  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2