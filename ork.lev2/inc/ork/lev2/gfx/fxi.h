#pragma once

struct FxShaderParamBlock;
struct FxShaderParamBuffer;
struct FxShaderParamBufferMapping;
typedef std::shared_ptr<FxShaderParamBufferMapping> parambuffermappingptr_t;

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// FxInterface (interface for dealing with FX materials)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////


class FxInterface {
public:
  void BeginFrame();

  virtual int BeginBlock(FxShader* hfx, const RenderContextInstData& data) = 0;
  virtual bool BindPass(FxShader* hfx, int ipass)                          = 0;
  virtual bool BindTechnique(FxShader* hfx, const FxShaderTechnique* htek) = 0;
  virtual void EndPass(FxShader* hfx)                                      = 0;
  virtual void EndBlock(FxShader* hfx)                                     = 0;
  virtual void CommitParams(void)                                          = 0;

  virtual const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) = 0;
  virtual const FxShaderParam* parameter(FxShader* hfx, const std::string& name)    = 0;
  virtual const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name)    = 0;

  #if defined(ENABLE_SHADER_STORAGE)
  virtual const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) = 0;
  #endif

  virtual void BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bval)                          = 0;
  virtual void BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int ival)                            = 0;
  virtual void BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec)                        = 0;
  virtual void BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec)                        = 0;
  virtual void BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec)                        = 0;
  virtual void BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount) = 0;
  virtual void BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfA, const int icnt)   = 0;
  virtual void BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA)                                = 0;
  virtual void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat)                       = 0;
  virtual void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat)                       = 0;
  virtual void BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) = 0;
  virtual void BindParamU32(FxShader* hfx, const FxShaderParam* hpar, uint32_t uval)                                  = 0;
  virtual void BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex)                      = 0;

#if ! defined(__APPLE__)
  virtual void BindParamU64(FxShader* hfx, const FxShaderParam* hpar, uint64_t uval) {}
#endif
  
  void BindParamTex(FxShader* hfx, const FxShaderParam* hpar, const lev2::TextureAsset* tex);

  void BeginMaterialGroup(GfxMaterial* pmtl);
  void EndMaterialGroup();
  GfxMaterial* GetGroupCurMaterial() const { return mpGroupCurMaterial; }
  GfxMaterial* GetGroupMaterial() const { return mpGroupMaterial; }

  virtual bool LoadFxShader(const AssetPath& pth, FxShader* ptex) = 0;

  void InvalidateStateBlock(void);

  GfxMaterial* GetLastFxMaterial(void) const { return mpLastFxMaterial; }

  static void Reset();


  virtual FxShaderParamBuffer* createParamBuffer( size_t length ) { return nullptr; }
  virtual parambuffermappingptr_t mapParamBuffer(FxShaderParamBuffer*b,size_t base=0, size_t length=0) { return nullptr; }
  virtual void unmapParamBuffer(FxShaderParamBufferMapping* mapping) {}
  virtual void bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) {}

protected:
  FxInterface();
  virtual ~FxInterface() {}
  
  FxShader* mpActiveFxShader;
  GfxMaterial* mpLastFxMaterial;
  GfxMaterial* mpGroupMaterial;
  GfxMaterial* mpGroupCurMaterial;

private:
  virtual void DoBeginFrame() = 0;
  virtual void DoOnReset() {}
};
