////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/spatializer.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void SpatializerData::describeX(object::ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializerData::describeX(object::ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

spatializer_ptr_t PannerSpatializerData::createInstance() const {
  return std::make_shared<PannerSpatializer>();
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializer::configureBus(outbus_ptr_t bus) {
  auto layerdata = std::make_shared<LayerData>();
  auto stage     = layerdata->appendStage("PAN");
  stage->setNumIos(2, 2);
  _pannerBlock = stage->appendTypedBlock<PANNER2D>("PANNER");
  bus->setBusDSP(layerdata);
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializer::updateSpatialParams(
    const fmtx4& listenerMatrix,
    const fvec3& emitterPos) {
  if (!_pannerBlock)
    return;

  fmtx4 invListener = listenerMatrix.inverse();
  fvec4 relPos4     = fvec4(emitterPos, 1.0f).transform(invListener);
  fvec3 relPos(relPos4.x, relPos4.y, relPos4.z);

  float distance = relPos.magnitude();
  float azimuth  = atan2f(relPos.x, relPos.z);

  auto angleParam = _pannerBlock->paramByName("ANGLE");
  auto distParam  = _pannerBlock->paramByName("DISTANCE");
  if (angleParam)
    angleParam->_coarse = azimuth;
  if (distParam)
    distParam->_coarse = std::max(1.0f, distance);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::audio::singularity::SpatializerData, "SpatializerData");
ImplementReflectionX(ork::audio::singularity::PannerSpatializerData, "PannerSpatializerData");
