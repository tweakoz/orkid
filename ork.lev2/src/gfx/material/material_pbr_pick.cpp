////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr_pik = logger()->createChannel("mtlpbrPIK", fvec3(0.8, 0.8, 0.1), true);

fxpipeline_ptr_t PBRMaterial::_createFxPipelinePIK(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  OrkAssert(permu._stereo == false);
  this->_vars->makeValueForKey<bool>("requirePBRparams") = false;
  if (permu._instanced and this->_tek_PIK_RI_IN) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_PIK_RI_IN;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
    // pipeline->bindParam(this->_paramM, "RCFD_M"_crcsh);
    // pipeline->bindParam(this->_paramMROT, "RCFD_Model_Rot"_crcsh);
    pipeline->bindParam(this->_parPickID, "RCID_PickID"_crcsh);
  }
  ////////////////
  else { // non-instanced
    if (permu._skinned and this->_tek_PIK_SK_NI) {
      // OrkBreak();
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = this->_tek_PIK_SK_NI;
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
      pipeline->bindParam(this->_paramM, "RCFD_M"_crcsh);
      pipeline->bindParam(this->_paramMROT, "RCFD_Model_Rot"_crcsh);
      pipeline->bindParam(this->_parPickID, "RCID_PickID"_crcsh);
      // pipeline->_debugBreak = true;
    } else if (not permu._skinned and this->_tek_PIK_RI_NI) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = this->_tek_PIK_RI_NI;
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
      pipeline->bindParam(this->_paramM, "RCFD_M"_crcsh);
      pipeline->bindParam(this->_paramMROT, "RCFD_Model_Rot"_crcsh);
      pipeline->bindParam(this->_parPickID, "RCID_PickID"_crcsh);
    } else {
      OrkAssert(false);
    }
    ////////////////
  }
  return pipeline;
}

} // namespace ork::lev2
