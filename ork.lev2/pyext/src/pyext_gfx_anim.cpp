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
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto anim_type_t = py::class_<XgmAnim, xgmanim_ptr_t>(module_lev2, "XgmAnim") //
      .def(py::init([](const std::string& anim_path) -> xgmanim_ptr_t {
        auto loadreq    = std::make_shared<asset::LoadRequest>(anim_path.c_str());
        auto anim_asset = asset::AssetManager<XgmAnimAsset>::load(loadreq);
        return anim_asset->_animation;
      }))
      .def_property_readonly(
          "numJointChannels", //
          [](xgmanim_ptr_t anim) -> size_t { //
            return anim->_jointanimationchannels.size();
          })
      .def_property_readonly(
          "numMaterialChannels", //
          [](xgmanim_ptr_t anim) -> size_t { //
            return anim->mMaterialAnimationChannels.size();
          });
  type_codec->registerStdCodec<xgmanim_ptr_t>(anim_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animinst_type_t = py::class_<XgmAnimInst, xgmaniminst_ptr_t>(module_lev2, "XgmAnimInst") //
      .def(py::init([]() -> xgmaniminst_ptr_t {
        return std::make_shared<XgmAnimInst>();
      }))
      .def(py::init([](xgmanim_constptr_t anim) -> xgmaniminst_ptr_t {
        auto rval = std::make_shared<XgmAnimInst>();
        rval->bindAnim(anim);
        return rval;
      }))
      .def(
          "bindAnim", //
          [](xgmaniminst_ptr_t self, xgmanim_constptr_t anim) { //
            self->bindAnim(anim);
          })
      .def_property_readonly(
          "mask", //
          [](xgmaniminst_ptr_t self) -> xgmanimmask_ptr_t { //
            return self->_mask;
          })
      .def_property_readonly(
          "sampleRate", //
          [](xgmaniminst_ptr_t self) -> float { //
            return self->GetSampleRate();
          })
      .def_property_readonly(
          "numFrames", //
          [](xgmaniminst_ptr_t self) -> size_t { //
            return self->numFrames();
          })
      .def_property_readonly(
          "weight", //
          [](xgmaniminst_ptr_t self) -> float { //
            return self->GetWeight();
          })
      .def_property(
          "use_temporal_lerp", //
            [](xgmaniminst_ptr_t self) -> bool { //
                return self->_use_temporal_lerp;
            },
            [](xgmaniminst_ptr_t self, bool bv) { //
                self->_use_temporal_lerp = bv;
            });
         
  type_codec->registerStdCodec<xgmaniminst_ptr_t>(animinst_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto animmask_type_t = py::class_<XgmAnimMask, xgmanimmask_ptr_t>(module_lev2, "XgmAnimMask") //
      .def_property_readonly("bytes",[](xgmanimmask_ptr_t self) -> std::string {
        std::string rval;
        for( int i=0; i<XgmAnimMask::knummaskbytes; i++ ){
           rval += FormatString( "%02x", self->mMaskBits[i] );
           if( i != (XgmAnimMask::knummaskbytes-1) )
             rval += ":";
        }
        return rval;
      })
      .def("enableAll",[](xgmanimmask_ptr_t self){
          self->EnableAll();
      })
      .def("disableAll",[](xgmanimmask_ptr_t self){
          self->DisableAll();
      })
      .def("enableBone",[](xgmanimmask_ptr_t self, xgmskeleton_constptr_t skeleton, const std::string& namedBone){
          self->Enable(skeleton,namedBone);
      })
      .def("disableBone",[](xgmanimmask_ptr_t self, xgmskeleton_constptr_t skeleton, const std::string& namedBone){
          self->Disable(skeleton,namedBone);
      })
      .def("enableBoneIndex",[](xgmanimmask_ptr_t self, int bone_index){
          self->Enable(bone_index);
      })
      .def("disableBoneIndex",[](xgmanimmask_ptr_t self, int bone_index){
          self->Disable(bone_index);
      });
  type_codec->registerStdCodec<xgmanimmask_ptr_t>(animmask_type_t);
  /////////////////////////////////////////////////////////////////////////////////
}


}