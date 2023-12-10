////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/string/string.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/util/logger.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/kernel/environment.h>

static bool SHOW_SKELETON() {
  return ork::genviron.has("ORKID_LEV2_SHOW_SKELETON");
}

namespace ork::lev2 {
static logchannel_ptr_t logchan_model = logger()->createChannel("model",fvec3(0.9,0.2,0.9),false);
///////////////////////////////////////////////////////////////////////////////

void ModelDrawableData::describeX(object::ObjectClass* clazz){
  clazz->directProperty("assetpath", &ModelDrawableData::_assetpath);
  clazz->directMapProperty("assetvars", &ModelDrawableData::_assetvars);
}

ModelDrawableData::ModelDrawableData(AssetPath path) : _assetpath(path) {
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t ModelDrawableData::createDrawable() const {
  auto drw = std::make_shared<ModelDrawable>(nullptr);
  drw->_data = this;
  drw->bindModelAsset(_assetpath);
  drw->_modcolor = _modcolor;
  drw->_name = _assetpath.c_str();
  return drw;
}

///////////////////////////////////////////////////////////////////////////////
ModelDrawable::ModelDrawable(DrawableOwner* pent) {
}
/////////////////////////////////////////////////////////////////////
ModelDrawable::~ModelDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
void ModelDrawable::bindModelInst(xgmmodelinst_ptr_t minst) {
  logchan_model->log("drw<%s> bindModelInst(%p)", _name.c_str(), minst.get() );
  _modelinst                  = minst;
  const lev2::XgmModel* Model = _modelinst->xgmModel();
  bool isSkinned              = Model->isSkinned();
  if (isSkinned) {
    _worldpose = std::make_shared<XgmWorldPose>(Model->_skeleton);
  }
  Drawable::var_t ap;
  ap.set(_worldpose);
  SetUserDataA(ap);
}
///////////////////////////////////////////////////////////////////////////////
asset::loadrequest_ptr_t ModelDrawable::bindModelAsset(AssetPath assetpath) {
  auto load_req = std::make_shared<asset::LoadRequest>(assetpath);
  bindModelAsset(load_req);
  return load_req;
}
///////////////////////////////////////////////////////////////////////////////
asset::loadrequest_ptr_t ModelDrawable::bindModelAsset( AssetPath assetpath, asset::vars_t asset_vars){

  auto load_req = std::make_shared<asset::LoadRequest>(assetpath);
  load_req->_asset_vars = asset_vars;
  bindModelAsset(load_req);
  return load_req;
}
///////////////////////////////////////////////////////////////////////////////
void ModelDrawable::bindModelAsset( asset::loadrequest_ptr_t load_req){

  auto assetpath = load_req->_asset_path;

  logchan_model->log("drw<%s> bindModelAsset(%s)", _name.c_str(), assetpath.c_str() );

  ork::opq::assertOnQueue(opq::mainSerialQueue());

  if (_data) {

    auto& asset_vars = load_req->_asset_vars;

    for (auto item : _data->_assetvars) {

      const std::string& k = item.first;
      const rendervar_t& v = item.second;

      std::string v_str;
      if (auto as_str = v.tryAs<std::string>()) {
        asset_vars.makeValueForKey<std::string>(k, as_str.value());
        v_str = as_str.value();
      } else if (auto as_bool = v.tryAs<bool>()) {
        asset_vars.makeValueForKey<bool>(k, as_bool.value());
        v_str = as_bool.value() ? "true" : "false";
      } else if (auto as_dbl = v.tryAs<double>()) {
        asset_vars.makeValueForKey<double>(k, as_dbl.value());
        v_str = FormatString("%f", as_dbl.value());
      } else {
        OrkAssert(false);
      }

      logchan_model->log("modelassetvar k<%s> v<%s>", k.c_str(), v_str.c_str());
    }
  }

  
  _asset = asset::AssetManager<XgmModelAsset>::load(load_req);
  bindModel(_asset->_model.atomicCopy());
}
///////////////////////////////////////////////////////////////////////////////
void ModelDrawable::bindModelAsset(xgmmodelassetptr_t asset) {
  _asset = asset;
  bindModel(_asset->_model.atomicCopy());
}
///////////////////////////////////////////////////////////////////////////////
void ModelDrawable::bindModel(xgmmodel_ptr_t model) {
  logchan_model->log("drw<%s> bindModel(%p)", _name.c_str(), (void*) model.get() );
  _model         = model;
  auto modelinst = std::make_shared<XgmModelInst>(_model.get());
  bindModelInst(modelinst);
}
///////////////////////////////////////////////////////////////////////////////
void ModelDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto RCFD                   = renderer->GetTarget()->topRenderContextFrameData();
  const auto& topCPD          = RCFD->topCPD();
  const lev2::XgmModel* Model = _modelinst->xgmModel();
  const auto& monofrustum     = topCPD.monoCamFrustum();

  // TODO - resolve frustum in case of stereo camera

  const ork::fmtx4 matw         = item->mXfData._worldTransform->composed();
  bool isPickState              = RCFD->_renderingmodel._modelID == "PICKING"_crcu;
  bool isSkinned                = Model->isSkinned();
  ork::fvec3 center_plus_offset = _offset + Model->boundingCenter();
  ork::fvec3 ctr                = ork::fvec4(center_plus_offset * _scale).transform(matw);
  ork::fvec3 vwhd               = Model->boundingAA_WHD();
  float frad                    = vwhd.x;
  if (vwhd.y > frad)
    frad = vwhd.y;
  if (vwhd.z > frad)
    frad = vwhd.z;
  frad *= 0.6f;

  if( isPickState ){
    if( not _pickable ){
      return;
    }
  }

  bool bCenterInFrustum = monofrustum.contains(ctr);

  //////////////////////////////////////////////////////////////////////

  ork::fvec3 matw_trans;
  ork::fquat matw_rot;
  float matw_scale;

  matw.decompose(matw_trans, matw_rot, matw_scale);

  int inumacc = 0;
  int inumrej = 0;

  //////////////////////////////////////////////////////////////////////

  auto do_submesh = [&](xgmsubmeshinst_ptr_t submeshinst) {
    auto submesh = submeshinst->_submesh;

    auto material = submeshinst->material();

    int inumclus = submesh->_clusters.size();

    for (int ic = 0; ic < inumclus; ic++) {
      bool btest = true;

      auto cluster = submesh->cluster(ic);

      if (isSkinned) {

        float fdb = monofrustum._bottomPlane.pointDistance(ctr);
        float fdt = monofrustum._topPlane.pointDistance(ctr);
        float fdl = monofrustum._leftPlane.pointDistance(ctr);
        float fdr = monofrustum._rightPlane.pointDistance(ctr);
        float fdn = monofrustum._nearPlane.pointDistance(ctr);
        float fdf = monofrustum._farPlane.pointDistance(ctr);

        const float kdist = -5.0f;
        btest             = (fdb > kdist) && (fdt > kdist) && (fdl > kdist) && (fdr > kdist) &&
                (fdn > kdist)
                //&&  (fdn<100.0f); // 50m actors
                && (fdf > kdist);
        if (false == btest) {
        }
        btest = true; // todo fix culler
      } else {        // Rigid
        const Sphere& bsph = cluster->mBoundingSphere;

        float clussphrad = bsph.mRadius * matw_scale * _scale;
        fvec3 clussphctr = ((bsph.mCenter + _offset) * _scale).transform(matw);
        Sphere sph2(clussphctr, clussphrad);

        btest = true; // CollisionTester::FrustumSphereTest( frus, sph2 );
      }

      if (btest) {
        //OrkBreak();
        lev2::ModelRenderable& renderable = renderer->enqueueModel();
        
        renderable._modelinst = _modelinst;
        renderable._pickID = _pickID;
        renderable._submeshinst = submeshinst;
        renderable._cluster = cluster;
        renderable.SetModColor(_modcolor);
        renderable.SetMatrix(matw);
        // renderable.SetLightMask(lmask);
        renderable._scale = _scale;
        renderable._orientation = _orientation;
        renderable._offset = _offset;

        renderable._sortkey = _sortkey;

        if (item->_onrenderable) {
          item->_onrenderable(&renderable);
        }

        inumacc++;
      } else {
        inumrej++;
      }
    }
  };
  //////////////////////////////////////////////////////////////////////

  for( auto submeshinst : _modelinst->_submeshinsts ){
    auto submesh = submeshinst->_submesh;
    if(submeshinst->_enabled)
      do_submesh(submeshinst);
  }
  if (_modelinst->_drawSkeleton or SHOW_SKELETON() and Model->isSkinned()) {
      auto& renderable = renderer->enqueueSkeleton();
      renderable._modelinst = _modelinst;
      renderable._pickID = _pickID;
      renderable.SetModColor(_modcolor);
      renderable.SetMatrix(matw);
      renderable._scale = _scale;
      renderable._orientation = _orientation;
      renderable._offset = _offset;
      renderable._sortkey = 0x7fffffff;
  }

}
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
ModelRenderable::ModelRenderable(IRenderer* renderer) {
}
///////////////////////////////////////////////////////////////////////////////
void ModelRenderable::Render(const IRenderer* renderer) const {

  auto context = renderer->GetTarget();
  auto minst   = this->_modelinst;
  auto model   = minst->xgmModel();
  auto submesh = _submeshinst->_submesh;
  auto mesh = submesh->_parentmesh;

  context->debugPushGroup(FormatString("IRenderer::RenderModel model<%p> minst<%p>", model, minst.get()));
  /////////////////////////////////////////////////////////////
  float fscale        = this->_scale;
  const fvec3& offset = this->_offset;
  fmtx4 smat, tmat, rmat;
  smat.setScale(fscale);
  tmat.setTranslation(offset);
  //rmat.setRotateY(rotate.y + rotate.z);
  rmat.fromQuaternion(_orientation);
  fmtx4 wmat = this->GetMatrix();
  /////////////////////////////////////////////////////////////
  // compute world matrix
  /////////////////////////////////////////////////////////////
  fmtx4 nmat = fmtx4::multiply_ltor(tmat,rmat,smat,wmat);
  if (minst->isBlenderZup()) { // zup to yup conversion matrix
    fmtx4 rmatx, rmaty;
    rmatx.rotateOnX(3.14159f * -0.5f);
    rmaty.rotateOnX(3.14159f);
    nmat = fmtx4::multiply_ltor(rmatx,rmaty,nmat);
  }
  /////////////////////////////////////////////////////////////
  RenderContextInstData RCID;
  RenderContextInstModelData RCID_MD;
  auto RCFD = context->topRenderContextFrameData();
  RCID._RCFD = RCFD;
  RCID_MD.mMesh    = mesh;
  RCID_MD.mSubMesh = submesh;
  RCID_MD._cluster = this->_cluster;
  RCID.SetRenderer(renderer);
  RCID.setRenderable(this);
  RCID._pipeline_cache = _submeshinst->_fxpipelinecache;
  RCID._pickID = _pickID;
  // context->debugMarker(FormatString("toolrenderer::RenderModel isskinned<%d> owner_as_ent<%p>", int(model->isSkinned()),
  // as_ent));
  ///////////////////////////////////////
  // printf( "Renderer::RenderModel() rable<%p>\n", & ModelRen );
  //logchan_model->log("renderable<%p> fxlut(%p)", (void*) this, (void*) RCID._fx_instance_lut.get() );
  bool model_is_skinned = model->isSkinned();
  RCID._isSkinned       = model_is_skinned;
  RCID_MD.SetSkinned(model_is_skinned);
  RCID_MD.SetModelInst(minst);

  auto ObjColor = this->_modColor;
  if (model_is_skinned) {
    model->RenderSkinned(minst.get(), ObjColor, nmat, context, RCID, RCID_MD);
  } else {
    model->RenderRigid(ObjColor, nmat, context, RCID, RCID_MD);
  }
  context->debugPopGroup();
}
/////////////////////////////////////////////////////////////////////
uint32_t ModelRenderable::ComposeSortKey(const IRenderer* renderer) const {
  return _sortkey;
}
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
SkeletonRenderable::SkeletonRenderable(IRenderer* renderer) {
}
///////////////////////////////////////////////////////////////////////////////
void SkeletonRenderable::Render(const IRenderer* renderer) const {

  auto context = renderer->GetTarget();
  auto minst   = this->_modelinst;
  auto model   = minst->xgmModel();
  context->debugPushGroup(FormatString("SkeletonRenderable model<%p> minst<%p>", model, minst.get()));
  /////////////////////////////////////////////////////////////
  float fscale        = this->_scale;
  const fvec3& offset = this->_offset;
  fmtx4 smat, tmat, rmat;
  smat.setScale(fscale);
  tmat.setTranslation(offset);
  rmat.fromQuaternion(_orientation);
  fmtx4 wmat = this->GetMatrix();
  /////////////////////////////////////////////////////////////
  // compute world matrix
  /////////////////////////////////////////////////////////////
  fmtx4 nmat = fmtx4::multiply_ltor(tmat,rmat,smat,wmat);
  if (minst->isBlenderZup()) { // zup to yup conversion matrix
    fmtx4 rmatx, rmaty;
    rmatx.rotateOnX(3.14159f * -0.5f);
    rmaty.rotateOnX(3.14159f);
    nmat = fmtx4::multiply_ltor(rmatx,rmaty,nmat);
  }
  /////////////////////////////////////////////////////////////
  RenderContextInstData RCID;
  auto RCFD = context->topRenderContextFrameData();
  RCID._RCFD = RCFD;
  RCID.SetRenderer(renderer);
  RCID.setRenderable(this);
  //RCID._pipeline_cache = _submeshinst->_fxpipelinecache;
  RCID._pickID = _pickID;
  OrkAssert( model->isSkinned() );
  RCID._isSkinned       = true;
  model->RenderSkeleton(minst.get(), nmat, context, RCID);
  context->debugPopGroup();
}
/////////////////////////////////////////////////////////////////////
uint32_t SkeletonRenderable::ComposeSortKey(const IRenderer* renderer) const {
  return _sortkey;
}
/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
ImplementReflectionX(ork::lev2::ModelDrawableData, "ModelDrawableData");
