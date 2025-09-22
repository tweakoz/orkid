////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { namespace lev2 {

using fxuniformset_byname_map_t   = std::unordered_map<std::string, const FxUniformBlock*>;
using fxsamplerset_byname_map_t   = std::unordered_map<std::string, fxsamplerset_constptr_t>;
using fxuniformblock_byname_map_t = std::unordered_map<std::string, fxuniformblock_constptr_t>;
using parambynamemap_t            = std::map<std::string, fxparam_constptr_t>;
using techniquebynamemap_t        = std::map<std::string, fxtechnique_constptr_t>;
using fxcompute_byname_map_t      = std::map<std::string, const FxComputeShader*>;
using fxstorageblock_byname_map_t = std::unordered_map<std::string, const FxShaderStorageBlock*>;

///////////////////////////////////////////////////////////////////////////////

class FxParamRec {
public:
  enum EBindingScope {
    ESCOPE_CONSTANT = 0,
    ESCOPE_PERFRAME,
    ESCOPE_PERMATERIALINST,
    ESCOPE_PEROBJECT,
  };

  FxParamRec();

  std::string _name;
  std::string mParameterSemantic;
  EPropType meParameterType;

  fxparam_constptr_t mParameterHandle;
  EBindingScope meBindingScope;
  U32 mTargetHash;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderPass {

  FxShaderPass();

  std::string _name;
  svarshp_t _impl;
  RenderQueueSortingData mRenderQueueSortingData;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechnique {

  bool _validated        = false;
  fxshader_ptr_t _shader = nullptr;
  std::string _techniqueName;
  orkvector<FxShaderPass*> _passes;
  svarshp_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderParamInBlockInfo {
  FxUniformBlock* _parent = nullptr;
};

struct FxShaderParam {

  FxShaderParam();

  std::string _name;
  std::string mParameterSemantic;
  std::string mParameterType;
  EPropType meParamType;
  svarshp_t _impl;
  bool mBindable;
  FxShaderParamInBlockInfo* _blockinfo = nullptr;
  FxShaderParam* mChildParam;

  orklut<std::string, std::string> _annotations;
};

struct FxUniformBlock {
  std::string _name;
  FxShaderParam* param(const std::string& name) const;
  std::map<std::string, FxShaderParam*> _subparams;
  svarshp_t _impl;
  FxInterface* _fxi = nullptr;
};
struct FxUniformSet {
  std::map<std::string, fxparam_constptr_t> _parametersByName;
};
struct FxSamplerSet {
  std::map<std::string, fxparam_constptr_t> _parametersByName;
};

struct FxUniformBuffer {
  size_t _length = 0;
  svarshp_t _impl;
};

struct FxUniformBufferMapping {
  FxUniformBufferMapping();
  ~FxUniformBufferMapping();
  void unmap();
  FxUniformBuffer* _buffer = nullptr;
  FxInterface* _fxi        = nullptr;
  size_t _offset           = 0;
  size_t _cursor           = 0;
  size_t _length           = 0;
  svarshp_t _impl;
  ///////////////////////////////////////////////////
  template <typename T> T& ref(size_t offset) {
    size_t end = offset + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + offset);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  template <typename T, typename... A> T& make(A&&... args) {
    size_t end = _cursor + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + _cursor);
    new (tstar) T(std::forward<A>(args)...);
    _cursor += sizeof(T);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  void seek(size_t offset) {
    _cursor = offset;
  }
  ///////////////////////////////////////////////////
  void* _mappedaddr = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Buffer member descriptor - describes a single field in a storage/uniform buffer
///////////////////////////////////////////////////////////////////////////////
struct FxBufferMember {
  std::string _name;        // Member identifier (e.g., "_lightcolor")
  std::string _datatype;    // GLSL type (e.g., "vec4", "mat4", "uint")
  size_t _offset = 0;       // Byte offset from buffer start
  size_t _size = 0;         // Size of single element in bytes
  size_t _stride = 0;       // Bytes between array elements (0 if not array)
  bool _is_array = false;   // True if this is an array
  size_t _array_length = 0; // Number of array elements (0 if not array)

  // Helper to calculate total size
  size_t totalSize() const {
    return _is_array ? (_stride * _array_length) : _size;
  }

  // Helper to get offset of array element
  size_t elementOffset(size_t index) const {
    OrkAssert(_is_array && index < _array_length);
    return _offset + (index * _stride);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Storage block descriptor - describes an SSBO
///////////////////////////////////////////////////////////////////////////////
struct FxShaderStorageBlock {
  std::string _name;                                                    // Storage interface name
  size_t _buffer_size = 0;                                             // Total buffer size in bytes
  std::unordered_map<std::string, fxbuffer_member_ptr_t> _members;    // Buffer members by name
  svarshp_t _impl;                                                      // Platform-specific implementation

  // Helper to find member by name - O(1) lookup
  fxbuffer_member_constptr_t findMember(const std::string& name) const {
    auto it = _members.find(name);
    return (it != _members.end()) ? it->second : nullptr;
  }
};
struct FxShaderStorageBuffer {
  size_t _length = 0;
  svarshp_t _impl;
};
struct FxShaderStorageBufferMapping {
  FxShaderStorageBufferMapping();
  ~FxShaderStorageBufferMapping();
  void unmap();
  FxShaderStorageBuffer* _buffer = nullptr;
  FxInterface* _fxi              = nullptr;
  size_t _offset                 = 0;
  size_t _cursor                 = 0;
  size_t _length                 = 0;
  svarshp_t _impl;

  template <typename T> T& ref(size_t offset) {
    size_t end = offset + sizeof(T);
    assert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + offset);
    return *tstar;
  }

  static int alignTo(int num, int alignment) {
    return ((num + alignment - 1) / alignment) * alignment;
  }

  ///////////////////////////////////////////////////
  void align(int quanta) {
    _cursor = alignTo(_cursor, quanta);
  }
  ///////////////////////////////////////////////////
  template <typename T, typename... A> T& advance() {
    T* tstar   = (T*)(((char*)_mappedaddr) + _cursor);
    size_t end = _cursor + sizeof(T);
    _cursor    = alignTo(end, 16);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  template <typename T, typename... A> T& make(A&&... args) {

    // statically assert T is not int, int32_t is ok
    static_assert(
        std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, fvec2> || std::is_same_v<T, fvec3> || std::is_same_v<T, fvec4> || std::is_same_v<T, fmtx3> ||
            std::is_same_v<T, fmtx4> || std::is_same_v<T, bool>,
        "Type T must be one of: float, double, int, int64_t, or uint32_t");

    switch (sizeof(T)) { // std430 layout rules
      case 4: {          // int32_t, uint32_t, float
        _cursor = alignTo(_cursor, 4);
        break;
      }
      case 8: { // double, fvec2
        _cursor = alignTo(_cursor, 8);
        break;
      }
      case 12: { // vec3
        _cursor = alignTo(_cursor, 16);
        break;
      }
      case 16: { // vec4
        _cursor = alignTo(_cursor, 16);
        break;
      }
      case 48: { // mat3
        _cursor = alignTo(_cursor, 16);
        break;
      }
      case 64: { // mat4
        _cursor = alignTo(_cursor, 16);
        break;
      }
      default:
        OrkAssert(false);
        break;
    }

    size_t end = _cursor + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + _cursor);
    new (tstar) T(std::forward<A>(args)...);
    _cursor += sizeof(T);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  void seek(size_t offset) {
    _cursor = offset;
  }

  void* _mappedaddr = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// TODO : implement descriptor sets at public API level
//  so we can hoist static descriptor binding code out of rendering loop
///////////////////////////////////////////////////////////////////////////////

struct FxShaderDescriptorSet {
  std::unordered_map<std::string, fxdescriptorsetitem_ptr_t> _items_by_name;
  std::unordered_map<fxparam_constptr_t, fxdescriptorsetitem_ptr_t> _items_by_param;
  std::unordered_map<int, fxdescriptorsetitem_ptr_t> _items_by_binding;
  svarshp_t _impl;
};
struct FxShaderDescriptorSetItem {
  svarshp_t _impl;
};
struct FxShaderDescriptorSetBindPoint {
  svarshp_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct FxComputeShader {
  svar64_t _impl;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShader {

  using parambynamemap_t      = std::map<std::string, fxparam_constptr_t>;
  using uniformblockbynamemap_t = std::map<std::string, const FxUniformBlock*>;
  using techniquebynamemap_t  = std::map<std::string, fxtechnique_constptr_t>;
  using computebynamemap_t    = std::map<std::string, const FxComputeShader*>;

  void OnReset();

  static void SetLoaderTarget(Context* targ);

  FxShader();

  static void RegisterLoaders(const file::Path& base, const std::string& ext);

  static const char* assetTypeNameStatic(void) {
    return "fxshader";
  }

  void addTechnique(fxtechnique_constptr_t tek);
  void addParameter(fxparam_constptr_t param);
  void addUniformBlock(const FxUniformBlock* block);
  void addComputeShader(const FxComputeShader* csh);

  const techniquebynamemap_t& techniques(void) const {
    return _techniques;
  }
  const parambynamemap_t& namedParams(void) const {
    return _parameterByName;
  }
  const uniformblockbynamemap_t& namedUniformBlocks(void) const {
    return _uniformBlockByName;
  }
  const computebynamemap_t& namedComputeShaders(void) const {
    return _computeShaderByName;
  }

  FxShaderParam* FindParamByName(const std::string& named);
  FxUniformBlock* FindParamBlockByName(const std::string& named);
  FxShaderTechnique* FindTechniqueByName(const std::string& named);

  FxComputeShader* findComputeShader(const std::string& named);

  void SetAllowCompileFailure(bool bv) {
    mAllowCompileFailure = bv;
  }
  bool GetAllowCompileFailure() const {
    return mAllowCompileFailure;
  }
  void SetFailedCompile(bool bv) {
    mFailedCompile = bv;
  }
  bool GetFailedCompile() const {
    return mFailedCompile;
  }

  void SetName(const char*);
  const char* GetName();

  ////////////////////////////////////////////////////
  // SSBO support
  ////////////////////////////////////////////////////

  const fxstorageblock_byname_map_t& namedStorageBlocks() const;
  void addStorageBlock(const FxShaderStorageBlock* block);
  FxShaderStorageBlock* storageBlockByName(const std::string& named);

  ////////////////////////////////////////////////////

  svar16_t _internalHandle;
  techniquebynamemap_t _techniques;
  parambynamemap_t _parameterByName;

  uniformblockbynamemap_t _uniformBlockByName; // TODO : aka _uniformBlocks
  computebynamemap_t _computeShaderByName;

  fxuniformset_byname_map_t _uniformSets;
  fxsamplerset_byname_map_t _samplerSets;
  fxuniformblock_byname_map_t _uniformBlocks;
  fxstorageblock_byname_map_t _storageBlockByName;

  ork::varmap::VarMap _varmap;

  bool mAllowCompileFailure = false;
  bool mFailedCompile       = false;
  std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
