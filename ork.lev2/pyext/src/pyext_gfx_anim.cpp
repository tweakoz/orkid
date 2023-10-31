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
  auto type_codec                  = python::TypeCodec::instance();
  module_lev2.attr("animMaxBones") = kmaxbones;
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
                             .def_property_readonly(
                                 "weight",                             //
                                 [](xgmaniminst_ptr_t self) -> float { //
                                   return self->GetWeight();
                                 })
                             .def_property(
                                 "use_temporal_lerp",                 //
                                 [](xgmaniminst_ptr_t self) -> bool { //
                                   return self->_use_temporal_lerp;
                                 },
                                 [](xgmaniminst_ptr_t self, bool bv) { //
                                   self->_use_temporal_lerp = bv;
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
                                   return self->GetJointName(index);
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
                                 });
  type_codec->registerStdCodec<xgmskeleton_ptr_t>(animskel_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lpose_type_t =
      py::class_<XgmLocalPose, xgmlocalpose_ptr_t>(module_lev2, "XgmLocalPose")
          .def(py::init([](xgmskeleton_ptr_t skel) -> xgmlocalpose_ptr_t { return std::make_shared<XgmLocalPose>(skel); }))
          .def("identityPose", [](xgmlocalpose_ptr_t self) { return self->identityPose(); })
          .def("bindPose", [](xgmlocalpose_ptr_t self) { return self->bindPose(); })
          .def("blendPoses", [](xgmlocalpose_ptr_t self) { return self->blendPoses(); })
          .def("concatenate", [](xgmlocalpose_ptr_t self) { return self->concatenate(); })
          .def_property_readonly("numJoints", [](xgmlocalpose_ptr_t self) -> int { return self->NumJoints(); })
          .def_property_readonly(
              "objSpaceBoundingSphere", [](xgmlocalpose_ptr_t self) -> fvec4 { return self->mObjSpaceBoundingSphere; });
  type_codec->registerStdCodec<xgmlocalpose_ptr_t>(lpose_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto wpose_type_t =
      py::class_<XgmWorldPose, xgmworldpose_ptr_t>(module_lev2, "XgmWorldPose")
          .def(py::init([](xgmskeleton_ptr_t skel) -> xgmworldpose_ptr_t { return std::make_shared<XgmWorldPose>(skel); }))
          .def("fromLocalPose", [](xgmworldpose_ptr_t self, xgmlocalpose_ptr_t lpose, fmtx4& world_matrix) {
            return self->apply(world_matrix, lpose);
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
              });
  type_codec->registerStdCodec<bone_transformer_ptr_t>(bxf_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto ikc_type_t = py::class_<IkChain, ikchain_ptr_t>(module_lev2, "IkChain")
  .def(py::init([](xgmskeleton_ptr_t skel) -> ikchain_ptr_t { //
    return std::make_shared<IkChain>(skel);
  }))
  .def(
      "bindToBone",                               //
      [](ikchain_ptr_t self, std::string named) { //
        return self->bindToBone(named);
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
      });
  type_codec->registerStdCodec<ikchain_ptr_t>(ikc_type_t);

  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2