////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::FreestyleMaterial() {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDepthTest::LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECullTest::PASS_BACK);
  miNumPasses = 1;
}
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::~FreestyleMaterial() {
}
///////////////////////////////////////////////////////////////////////////////

static fxinstance_ptr_t _createFxStateInstance(const FxCachePermutation& permu, //
                                               const FreestyleMaterial*mtl){

  fxinstance_ptr_t fxinst = nullptr;

  switch (mtl->_variant) {
    case "FORWARD_UNLIT"_crcu:
    case "CUSTOM"_crcu:
    case 0: { // free-freestyle
      fxinst             = std::make_shared<FxStateInstance>(permu);

      fxinst->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
        auto _this       = (FreestyleMaterial*)mtl;
        auto RCFD        = RCID._RCFD;
        auto context     = RCFD->GetTarget();
        auto RSI         = context->RSI();
        RSI->BindRasterState(_this->_rasterstate);
      });
       break;
    }
    default:
      printf( "UNKNOWN VARIANT<%zu>\n", mtl->_variant);
      OrkAssert(false);
      break;
  }
  if(permu._forced_technique){
    fxinst->_technique = permu._forced_technique;
  }
  return fxinst;
}

///////////////////////////////////////////////////////////////////////////////

using cache_impl_t = FxStateInstanceCacheImpl<FreestyleMaterial>;

using freestylecache_impl_ptr_t = std::shared_ptr<cache_impl_t>;

static freestylecache_impl_ptr_t _getfreestylecache(){
  static freestylecache_impl_ptr_t _gcache = std::make_shared<cache_impl_t>();
  return _gcache;
}

fxinstancecache_constptr_t FreestyleMaterial::_doFxInstanceCache(fxcachepermutation_set_constptr_t perms) const { // final
  return _getfreestylecache()->getCache(this);
}


///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::dump() const {

  printf("freestylematerial<%p>\n", (void*) this);
  printf("fxshader<%p>\n", (void*) _shader);

  for (auto item : _shader->_techniques) {

    auto name = item.first;
    auto tek  = item.second;
    printf("tek<%p:%s> valid<%d>\n", (void*) tek, name.c_str(), int(tek->mbValidated));
  }
  for (auto item : _shader->_parameterByName) {
    auto name = item.first;
    auto par  = item.second;
    printf("par<%p:%s> type<%s>\n", (void*) par, name.c_str(), par->mParameterType.c_str());
  }
  for (auto item : _shader->_parameterBlockByName) {
    auto name   = item.first;
    auto parblk = item.second;
    printf("parblk<%p:%s>\n", (void*) parblk, name.c_str());
  }
  for (auto item : _shader->_computeShaderByName) {
    auto name = item.first;
    auto csh  = item.second;
    printf("csh<%p:%s>\n", (void*) csh, name.c_str());
  }
}
///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::Update() { // final
}
///////////////////////////////////////////////////////////////////////////////
bool FreestyleMaterial::BeginPass(Context* targ, int iPass) { // final
  return targ->FXI()->BindPass(iPass);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndPass(Context* targ) { // final
  targ->FXI()->EndPass();
}
///////////////////////////////////////////////////////////////////////////////
int FreestyleMaterial::BeginBlock(Context* targ, const RenderContextInstData& RCID) { // final
  auto fxi    = targ->FXI();
  int npasses = fxi->BeginBlock(_selectedTEK, RCID);
  return npasses;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndBlock(Context* targ) { // final
  targ->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
// new style interface
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::gpuInit(Context* targ) { // final
}
void FreestyleMaterial::gpuInit(Context* targ, const AssetPath& assetname) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    auto fxi       = targ->FXI();
    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = assetname.c_str();
    _shaderasset   = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader        = _shaderasset->GetFxShader();
    OrkAssert(_shader);
  }
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    auto loadreq = std::make_shared<asset::LoadRequest>();
    loadreq->_asset_path = "shaderFromShaderText";
    _shaderasset   = std::make_shared<FxShaderAsset>();
    _shaderasset->setRequest(loadreq);
    _shader = targ->FXI()->shaderFromShaderText(shadername, shadertext);
    OrkAssert(_shader);

    delete _shaderasset->_shader;
    _shaderasset->_shader = _shader;
  }
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderTechnique* FreestyleMaterial::technique(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto tek = fxi->technique(_shader, named);
  if (tek != nullptr)
    _techniques.insert(tek);
  return tek;
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderParam* FreestyleMaterial::param(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto par = fxi->parameter(_shader, named);
  if (par != nullptr)
    _params.insert(par);
  return par;
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderParamBlock* FreestyleMaterial::paramBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto par = fxi->parameterBlock(_shader, named);
  if (par != nullptr)
    _paramBlocks.insert(par);
  return par;
}
////////////////////////////////////////////
void FreestyleMaterial::commit() {
  auto fxi = _initialTarget->FXI();
  fxi->CommitParams();
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindTechnique(const FxShaderTechnique* tek) {
  _selectedTEK = tek;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamInt(const FxShaderParam* par, int value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamInt(par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamFloat(const FxShaderParam* par, float value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamFloat(par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamFloatArray(const FxShaderParam* par, const float* value, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamFloatArray(par, value,count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamCTex(const FxShaderParam* par, const Texture* tex) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamCTex(par, tex);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec2(const FxShaderParam* par, const fvec2& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect2(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec3(const FxShaderParam* par, const fvec3& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect3(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec4(const FxShaderParam* par, const fvec4& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect4(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamQuat(const FxShaderParam* par, const fquat& q) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fvec4 V4(q.w,q.x,q.y,q.z);
  fxi->BindParamVect4(par, V4);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamPlane(const FxShaderParam* par, const fplane& p) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fvec4 V4(p.n.x,p.n.y,p.n.z,p.d);
  fxi->BindParamVect4(par, V4);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec2Array(const FxShaderParam* par, const fvec2* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect2Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec3Array(const FxShaderParam* par, const fvec3* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect3Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec4Array(const FxShaderParam* par, const fvec4* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect4Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamU64(const FxShaderParam* par, uint64_t v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamU64(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx4& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrix(par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx3& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrix(par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrixArray(par, m, len);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(const FxShaderTechnique* tek, const RenderContextFrameData& RCFD) {
  OrkAssert(tek != nullptr);
  auto targ = RCFD.GetTarget();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();
  RenderContextInstData RCID(&RCFD);
  _selectedTEK = tek;
  int npasses  = this->BeginBlock(targ, RCID);
  fxi->BindPass(0);
  rsi->BindRasterState(_rasterstate);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(
    const FxShaderTechnique* tekMono,
    const FxShaderTechnique* tekStereo,
    const RenderContextFrameData& RCFD) {
  const auto& CPD = RCFD.topCPD();
  begin(CPD.isStereoOnePass() ? tekStereo : tekMono, RCFD);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::end(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  this->EndPass(targ);
  this->EndBlock(targ);
}
////////////////////////////////////////////////////////////////////////////////
// Compute Shaders ?
////////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
const FxShaderStorageBlock* FreestyleMaterial::storageBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto blk = fxi->storageBlock(_shader, named);
  if (blk != nullptr)
    _storageBlocks.insert(blk);
  return blk;
}
///////////////////////////////////////////////////////////////////////////////
const FxComputeShader* FreestyleMaterial::computeShader(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto tek = fxi->computeShader(_shader, named);
  if (tek != nullptr)
    _computeShaders.insert(tek);
  return tek;
}
#endif
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
