////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::FreestyleMaterial() {
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _rasterstate->setWriteMaskZ(true);
  _rasterstate->setCullTest(ECullTest::PASS_FRONT);
  miNumPasses = 1;
  _uservars = std::make_shared<varmap::VarMap>();
}
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::~FreestyleMaterial() {
}
///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t FreestyleMaterial::_createFxPipeline(const FxPipelinePermutation& permu, //
                                                      const FreestyleMaterial*mtl){

  fxpipeline_ptr_t pipeline = nullptr;

  switch (mtl->_variant) {
    case "FORWARD_UNLIT"_crcu:
    case "CUSTOM"_crcu:
    case 0: { // free-freestyle
      pipeline             = std::make_shared<FxPipeline>(permu);

      pipeline->addStateLambda([mtl](const RenderContextInstData& RCID) {
        auto _this       = (FreestyleMaterial*)mtl;
        auto RCFD        = RCID.rcfd();
        auto context     = RCFD->GetTarget();
        //auto RSI         = context->RSI();
        context->FXI()->applyRasterState(*(_this->_rasterstate));
      });
       break;
    }
    default:
      fprintf( stderr, "UNKNOWN VARIANT<%u>\n", mtl->_variant);
      OrkAssert(false);
      break;
  }
  if(permu._forced_technique){
    pipeline->_technique = permu._forced_technique;
  }
  if(pipeline){
    pipeline->_rasterstate = mtl->_rasterstate;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////

using cache_impl_t = FxPipelineCacheImpl<FreestyleMaterial>;

using freestylecache_impl_ptr_t = std::shared_ptr<cache_impl_t>;

static freestylecache_impl_ptr_t _getfreestylecache(){
  static freestylecache_impl_ptr_t _gcache = std::make_shared<cache_impl_t>();
  return _gcache;
}

fxpipelinecache_constptr_t FreestyleMaterial::_doFxPipelineCache(fxpipelinepermutation_set_constptr_t perms) const { // final
  return _getfreestylecache()->getCache(this);
}


///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::dump() const {
  printf("//////////////////////////////////\n");
  printf("freestylematerial<%p>\n", (void*) this);
  printf("fxshader<%p>\n", (void*) _shader);

  printf("techniques count<%zu>\n", _shader->_techniques.size());
  for (auto item : _shader->_techniques) {

    auto name = item.first;
    auto tek  = item.second;
    printf("  tek<%p:%s> valid<%d>\n", (void*) tek, name.c_str(), int(tek->_validated));
  }
  printf("parametersByName count<%zu>\n", _shader->_parameterByName.size());
  for (auto item : _shader->_parameterByName) {
    auto name = item.first;
    auto par  = item.second;
    printf("  par<%p:%s> type<%s>\n", (void*) par, name.c_str(), par->mParameterType.c_str());
  }
  printf(" uniformBlocksByName count<%zu>\n", _shader->_uniformBlockByName.size());
  for (auto item : _shader->_uniformBlockByName) {
    auto name   = item.first;
    auto parblk = item.second;
    printf("  parblk<%p:%s>\n", (void*) parblk, name.c_str());
  }
  printf(" computeShadersByName count<%zu>\n", _shader->_computeShaderByName.size());
  for (auto item : _shader->_computeShaderByName) {
    auto name = item.first;
    auto csh  = item.second;
    printf("  csh<%p:%s>\n", (void*) csh, name.c_str());
  }
  printf("//////////////////////////////////\n");
}
///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::Update() { // final
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
  else{
    fprintf(stderr,"FreestyleMaterial<%s> no param named<%s>\n", //
           _shader->mName.c_str(),                       //
           named.c_str());                               //
    fflush(stderr);
  }
  return par;
}
///////////////////////////////////////////////////////////////////////////////
const FxUniformBlock* FreestyleMaterial::uniformBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto par = fxi->uniformBlock(_shader, named);
  if (par != nullptr)
    _uniformBlocks.insert(par);
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
void FreestyleMaterial::bindParam(const FxShaderParam* param, const varval_t& val) {
    auto FXI = _initialTarget->FXI();


    if (auto as_mtx4 = val.tryAs<fmtx4>()) {
      FXI->bindParamMatrix(param, as_mtx4.value());
    } else if (auto as_mtx4ptr = val.tryAs<fmtx4_ptr_t>()) {
      FXI->bindParamMatrix(param, *as_mtx4ptr.value().get());
    } else if (auto as_texture = val.tryAs<Texture*>()) {
      auto texture = as_texture.value();
      FXI->bindParamTexture(param, texture);
    } else if (auto as_texture = val.tryAs<texture_ptr_t>()) {
      auto texture = as_texture.value();
      FXI->bindParamTexture(param, texture.get());
    } else if (auto as_texturearray = val.tryAs<texturearray_ptr_t>()) {
      auto texture = as_texturearray.value();
      FXI->bindParamTextureArray(param, texture.get());
    } else if (auto as_tex_provider = val.tryAs<texture_provider_ptr_t>()) {
      auto texture = as_tex_provider.value()->getTexture();
      FXI->bindParamTexture(param, texture.get());
    } else if (auto as_bool_ = val.tryAs<bool>()) {
      FXI->bindParamBool(param, as_bool_.value());
    } else if (auto as_float_ = val.tryAs<float>()) {
      FXI->bindParamFloat(param, as_float_.value());
    } else if (auto as_fvec4_ = val.tryAs<fvec4>()) {
      FXI->bindParamVect4(param, as_fvec4_.value());
    } else if (auto as_fvec3 = val.tryAs<fvec3>()) {
      FXI->bindParamVect3(param, as_fvec3.value());
    } else if (auto as_fvec2 = val.tryAs<fvec2>()) {
      FXI->bindParamVect2(param, as_fvec2.value());
    } else if (auto as_fmtx3 = val.tryAs<fmtx3>()) {
      FXI->bindParamMatrix(param, as_fmtx3.value());
    } else if (auto as_instancedata_ = val.tryAs<instanceddrawinstancedata_ptr_t>()) {
      OrkAssert(false);
    } else if (auto as_fquat = val.tryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      FXI->bindParamVect4(param, as_vec4);
    } else if (auto as_fplane3 = val.tryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      FXI->bindParamVect4(param, as_vec4);
    } 
 }
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamBool(const FxShaderParam* par, bool value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamBool(par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamInt(const FxShaderParam* par, int value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamInt(par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamFloat(const FxShaderParam* par, float value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamFloat(par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamFloatArray(const FxShaderParam* par, const float* value, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamFloatArray(par, value,count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamTexture(const FxShaderParam* par, const Texture* tex) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamTexture(par, tex);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamTextureArray(const FxShaderParam* par, const TextureArray* tex) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamTextureArray(par, tex);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec2(const FxShaderParam* par, const fvec2& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect2(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec3(const FxShaderParam* par, const fvec3& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect3(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec4(const FxShaderParam* par, const fvec4& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect4(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamQuat(const FxShaderParam* par, const fquat& q) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fvec4 V4(q.w,q.x,q.y,q.z);
  fxi->bindParamVect4(par, V4);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamPlane(const FxShaderParam* par, const fplane& p) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fvec4 V4(p.n.x,p.n.y,p.n.z,p.d);
  fxi->bindParamVect4(par, V4);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec2Array(const FxShaderParam* par, const fvec2* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect2Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec3Array(const FxShaderParam* par, const fvec3* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect3Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec4Array(const FxShaderParam* par, const fvec4* v, size_t count) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamVect4Array(par, v, count);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamU32(const FxShaderParam* par, uint64_t v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamU32(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamU64(const FxShaderParam* par, uint64_t v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamU64(par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx4& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamMatrix(par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx3& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamMatrix(par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->bindParamMatrixArray(par, m, len);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(const FxShaderTechnique* tek, rcfd_ptr_t RCFD) {
  OrkAssert(tek != nullptr);
 auto targ = RCFD->GetTarget();
  auto fxi  = targ->FXI();
  //auto rsi  = targ->RSI();
  RenderContextInstData RCID(RCFD);
  _selectedTEK = tek;
  int npasses  = this->BeginBlock(targ, RCID);
  fxi->applyRasterState(*_rasterstate);
  OrkAssert(tek->_validated);
 }
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(
    const FxShaderTechnique* tekMono,
    const FxShaderTechnique* tekStereo,
    rcfd_ptr_t RCFD) {
  const auto& CPD = RCFD->topCPD();
  begin(CPD.isSinglePassStereo() ? tekStereo : tekMono, RCFD);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::end(rcfd_ptr_t RCFD) {
  auto targ = RCFD->GetTarget();
  this->EndBlock(targ);
}
////////////////////////////////////////////////////////////////////////////////
const FxShaderStorageBlock* FreestyleMaterial::storageBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto blk = fxi->storageBlock(_shader, named);
  if (blk != nullptr)
    _storageBlocks.insert(blk);
  return blk;
}
////////////////////////////////////////////////////////////////////////////////
const FxComputeShader* FreestyleMaterial::computeShader(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto tek = fxi->computeShader(_shader, named);
  if (tek != nullptr)
    _computeShaders.insert(tek);
  return tek;
}
///////////////////////////////////////////////////////////////////////////////
lev2::freestyle_mtl_ptr_t createShaderFromFile(lev2::Context* ctx, std::string debugname, file::Path shader_path) {
  ork::File shader_file(shader_path, ork::EFM_READ);
  size_t length = 0;
  shader_file.GetLength(length);
  std::string shader_text;
  shader_text.resize(length + 1);
  shader_file.Read(shader_text.data(), length);
  shader_text.data()[length] = 0;

  auto mtl = std::make_shared<FreestyleMaterial>();
  mtl->gpuInitFromShaderText(ctx, debugname, shader_text);

  return mtl;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
