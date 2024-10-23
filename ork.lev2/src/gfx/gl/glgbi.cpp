////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/orkmath.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <stdlib.h>
#include <ork/lev2/ui/ui.h>
//#include <ork/lev2/gfx/modeler/modeler_base.h>

////////////////////////////////////////////////////////////////////////////////

static const bool USEVBO = true;
#define USEIBO 0

namespace ork { namespace lev2 {

////////////////////////////////////////////////////////////////////////////////
//	GL Vertex Buffer Implementation
///////////////////////////////////////////////////////////////////////////////

enum edynvbopath {
  EVB_BUFFER_SUBDATA = 0,
  EVB_MAP_BUFFER_RANGE,
#if defined(ORK_OSX)
  EVB_APPLE_FLUSH_RANGE, // seems broken
#endif
};

static edynvbopath gDynVboPath = EVB_BUFFER_SUBDATA;

void ContextGL::TakeThreadOwnership() {
  makeCurrentContext();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct GlVtxBufMapPool;
struct GlVtxBufMapData;

typedef ork::MpMcBoundedQueue<GlVtxBufMapData*> vtxbufmapdata_q_t;

///////////////////////////////////////////////////////////

struct GlVtxBufMapData {
  vtxbufmapdata_q_t& _parentQ;
  int mPotSize;
  int mCurSize;
  void* mpData;

  GlVtxBufMapData(vtxbufmapdata_q_t& oq, int potsize)
      : _parentQ(oq)
      , mPotSize(potsize)
      , mCurSize(0) {
    mpData = malloc(potsize);
  }
  ~GlVtxBufMapData() {
    free(mpData);
  }
  void ReturnToPool() {
    _parentQ.push(this);
  }
};

///////////////////////////////////////////////////////////

struct GlVtxBufMapPool {
  std::map<int, vtxbufmapdata_q_t> mVbmdMaps;

  GlVtxBufMapData* GetVbmd(int isize) {
    GlVtxBufMapData* rval = nullptr;
    int potsize           = 1;
    while (potsize < isize)
      potsize <<= 1;
    assert(potsize >= isize);
    std::map<int, vtxbufmapdata_q_t>::iterator it = mVbmdMaps.find(potsize);
    if (it == mVbmdMaps.end()) {
      vtxbufmapdata_q_t q;
      mVbmdMaps.insert(std::make_pair(potsize, q));
      it = mVbmdMaps.find(potsize);
    }
    vtxbufmapdata_q_t& q = it->second;
    if (false == q.try_pop(rval))
      rval = new GlVtxBufMapData(q, potsize);
    return rval;
  }
};

///////////////////////////////////////////////////////////

static ork::MpMcBoundedQueue<GlVtxBufMapPool*> gBufMapPool;

static GlVtxBufMapPool* GetBufMapPool() {
  GlVtxBufMapPool* rval = nullptr;
  if (false == gBufMapPool.try_pop(rval)) {
    rval = new GlVtxBufMapPool;
  }
  return rval;
}
static void RetBufMapPool(GlVtxBufMapPool* p) {
  assert(p != nullptr);
  gBufMapPool.push(p);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct GLIdxBufHandle {
  const U16* mBuffer = nullptr;

  uint32_t mIBO;
  int mNumIndices;
  int mMinIndex;
  int mMaxIndex;

  GLIdxBufHandle()
      : mIBO(0)
      , mNumIndices(0) 
      , mMinIndex(0)
      , mMaxIndex(0){
  }
};

struct GLVaoHandle {
  const GLIdxBufHandle* mIBO = nullptr;

  GLuint mVAO;
  bool mInited;

  GLVaoHandle()
      : mVAO(0)
      , mInited(false) {
  }
  ~GLVaoHandle() {
    if (mVAO) {
      glDeleteVertexArrays(1, &mVAO);
    }
  }
};

struct GLVtxBufHandle {
  u32 mVBO;
  long mBufSize;
  int miLockBase;
  int miLockCount;
  bool mbSetupSource;
  GlVtxBufMapData* mMappedRegion;

  std::map<size_t, GLVaoHandle*> mVaoMap;

  GLVtxBufHandle()
      : mVBO(0)
      , mBufSize(0)
      , miLockBase(0)
      , miLockCount(0)
      , mbSetupSource(true)
      , mMappedRegion(nullptr) {
  }
  ~GLVtxBufHandle() {
    if (mVBO) {
      glDeleteBuffers(1, &mVBO);
    }
    for( auto item : mVaoMap ){
      delete item.second;
    }
  }
  GLVaoHandle* GetVAO(const void* plat_h, const void* vao_key) {
    GLVaoHandle* rval = nullptr;
    size_t k1         = size_t(plat_h);
    size_t k2         = size_t(vao_key);
    size_t key        = k1 xor k2 xor size_t(this);
    const auto& it    = mVaoMap.find(key);
    if (it == mVaoMap.end()) {
      rval = new GLVaoHandle;
      glGenVertexArrays(1, &rval->mVAO);
      mVaoMap.insert(std::make_pair(key, rval));
      // glBindVertexArray(mVAO);
    } else
      rval = it->second;
    return rval;
  }
  GLVaoHandle* BindVao(const void* plat_h, const void* vao_key) {
    GLVaoHandle* r = GetVAO(plat_h, vao_key);
    assert(r != nullptr);
    glBindVertexArray(r->mVAO);
    return r;
  }
  void CreateVbo(VertexBufferBase& VBuf) {
    VertexBufferBase* pnonconst = const_cast<VertexBufferBase*>(&VBuf);

    // printf( "CreateVBO()\n");

    // Create A VBO and copy data into it
    mVBO = 0;
    glGenBuffers(1, (GLuint*)&mVBO);
    // printf( "CreateVBO:: genvbo<%d>\n", int(mVBO));

    // hPB = GLuint(ubh);
    // pnonconst->SetPBHandle( (void*)hPB );
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    GL_ERRORCHECK();
    int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();

    // orkprintf( "CreateVBO<%p> len<%d> ID<%d>\n", & VBuf, iVBlen, int(mVBO) );

    bool bSTATIC = VBuf.IsStatic();

    void* gzerobuf = calloc(iVBlen, 1);
    // glBufferData( GL_ARRAY_BUFFER, iVBlen, bSTATIC ? gzerobuf : 0, bSTATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW );
    glBufferData(GL_ARRAY_BUFFER, iVBlen, gzerobuf, bSTATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    free(gzerobuf);

    GL_ERRORCHECK();

    //////////////////////////////////////////////
    // we always update dynamic VBOs sequentially
    //  we also dont want to pay the cost of copying any data
    //  so we will use a VBO map range extension
    //  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range

    if (false == bSTATIC)
      switch (gDynVboPath) {
#if defined(__APPLE__)
        case EVB_APPLE_FLUSH_RANGE:
          break;
#endif
        case EVB_BUFFER_SUBDATA:
          break;
        case EVB_MAP_BUFFER_RANGE: {
          break;
        }
      }

    //////////////////////////////////////////////

    GLint ibufsize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &ibufsize);
    GL_ERRORCHECK();
    mBufSize = ibufsize;

    OrkAssert(mBufSize > 0);
  }
};

///////////////////////////////////////////////////////////////////////////////

GlGeometryBufferInterface::GlGeometryBufferInterface(ContextGL& target)
    : GeometryBufferInterface(target)
    , mTargetGL(target)
    , mLastComponentMask(0) {
}

static void ClearVao() {
  GL_ERRORCHECK();
  glBindVertexArray(0);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void* GlGeometryBufferInterface::LockVB(VertexBufferBase& VBuf, int ibase, int icount) {
  mTargetGL.makeCurrentContext();

  GL_ERRORCHECK();

  OrkAssert(false == VBuf.IsLocked());

  void* rVal           = 0;
  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

  //////////////////////////////////////////////////////////
  // create the vbo ?
  //////////////////////////////////////////////////////////
  if (0 == hBuf) {
    hBuf = new GLVtxBufHandle;
    VBuf.SetHandle(reinterpret_cast<void*>(hBuf));

    hBuf->CreateVbo(VBuf);
  }

  int iMax = VBuf.GetMax();

  int ibasebytes = ibase * VBuf.GetVtxSize();
  int isizebytes = icount * VBuf.GetVtxSize();

  GL_ERRORCHECK();

  ClearVao();

  //////////////////////////////////////////////////////////
  // bind the vbo
  //////////////////////////////////////////////////////////

  GL_ERRORCHECK();
  glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
  GL_ERRORCHECK();

  if (VBuf.IsStatic()) {
    if (isizebytes) {
      rVal = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
      OrkAssert(rVal);
    }
  } else {
    // printf( "LOCKVB<WRITE> VB<%p> vboid<%d> ibase<%d> icount<%d> isizebytes<%d> mBufSize<%d>\n", & VBuf, int(hBuf->mVBO), ibase,
    // icount, isizebytes, int(hBuf->mBufSize) );
    //////////////////////////////////////////////
    // we always update dynamic VBOs sequentially (no overrwrite)
    //  we also dont want to pay the cost of copying any data
    //  so we will use a VBO map range extension
    //  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range
    //////////////////////////////////////////////
    OrkAssert(isizebytes);

    if (isizebytes) {
      hBuf->miLockBase  = ibase;
      hBuf->miLockCount = icount;

      switch (gDynVboPath) {
#if defined(__APPLE__)
        case EVB_APPLE_FLUSH_RANGE:
          break;
#endif
        case EVB_MAP_BUFFER_RANGE:
          // rVal = glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_FLUSH_EXPLICIT_BIT ); //
          // MAP_UNSYNCHRONIZED_BIT?
          rVal = glMapBufferRange(
              GL_ARRAY_BUFFER,
              ibasebytes,
              isizebytes,
              GL_MAP_WRITE_BIT | 
              GL_MAP_INVALIDATE_RANGE_BIT | 
              GL_MAP_FLUSH_EXPLICIT_BIT |
              GL_MAP_UNSYNCHRONIZED_BIT); // MAP_UNSYNCHRONIZED_BIT?
          // rVal = glMapBufferRange( GL_ARRAY_BUFFER, ibasebytes, isizebytes, GL_MAP_WRITE_BIT ); // MAP_UNSYNCHRONIZED_BIT?
          GL_ERRORCHECK();
          OrkAssert(rVal);
          // rVal = (void*) (((char*)rVal)+ibasebytes);
          break;
        case EVB_BUFFER_SUBDATA: {
          GlVtxBufMapPool* pool = GetBufMapPool();
          hBuf->mMappedRegion   = pool->GetVbmd(isizebytes);
          assert(hBuf->mMappedRegion != nullptr);
          rVal = hBuf->mMappedRegion->mpData;
          RetBufMapPool(pool);
          break;
        }
      }
    }
  }

  //////////////////////////////////////////////////////////
  // boilerplate stuff all devices do
  //////////////////////////////////////////////////////////

  VBuf.Lock();

  //////////////////////////////////////////////////////////

  return rVal;
}

///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockVB(const VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());

  void* rVal           = 0;
  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

  OrkAssert(hBuf != 0);

  int iMax = VBuf.GetMax();

  if (icount == 0) {
    icount = VBuf.GetMax();
  }
  int ibasebytes = ibase * VBuf.GetVtxSize();
  int isizebytes = icount * VBuf.GetVtxSize();

  OrkAssert(isizebytes);

  // printf( "ibasebytes<%d> isizebytes<%d> icount<%d> \n", ibasebytes, isizebytes, icount );

  ClearVao();

  //////////////////////////////////////////////////////////
  // bind the vbo
  //////////////////////////////////////////////////////////

  GL_ERRORCHECK();
  glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
  GL_ERRORCHECK();

  //////////////////////////////////////////////////////////

  if (isizebytes) {
    rVal = glMapBufferRange(GL_ARRAY_BUFFER, ibasebytes, isizebytes, GL_MAP_READ_BIT); // MAP_UNSYNCHRONIZED_BIT?
    OrkAssert(rVal);
    OrkAssert(rVal != (void*)0xffffffff);
    hBuf->miLockBase  = ibase;
    hBuf->miLockCount = icount;
  }

  GL_ERRORCHECK();

  //////////////////////////////////////////////////////////

  VBuf.Lock();

  return rVal;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB(VertexBufferBase& VBuf) {
  OrkAssert(VBuf.IsLocked());

  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

  if (VBuf.IsStatic()) {
    GL_ERRORCHECK();

    glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    VBuf.Unlock();
  } else {
    int basebytes  = VBuf.GetVtxSize() * hBuf->miLockBase;
    int countbytes = VBuf.GetVtxSize() * hBuf->miLockCount;

    // printf( "UNLOCK VB<%p> base<%d> count<%d>\n", & VBuf, basebytes, countbytes );

    GL_ERRORCHECK();
    glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
    GL_ERRORCHECK();

    switch (gDynVboPath) {
#if defined(__APPLE__)
      case EVB_APPLE_FLUSH_RANGE:
        break;
#endif
      case EVB_MAP_BUFFER_RANGE:
        GL_ERRORCHECK();
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, (GLintptr)0, countbytes);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        break;
      case EVB_BUFFER_SUBDATA: {
        assert(hBuf->mMappedRegion != nullptr);
        glBufferSubData(GL_ARRAY_BUFFER, basebytes, countbytes, hBuf->mMappedRegion->mpData);
        hBuf->mMappedRegion->ReturnToPool();
        break;
      }
    }

    GL_ERRORCHECK();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GL_ERRORCHECK();

    VBuf.Unlock();
  }
  ClearVao();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB(const VertexBufferBase& VBuf) {
  OrkAssert(VBuf.IsLocked());
  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
  GL_ERRORCHECK();

  GL_ERRORCHECK();
  glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
  GL_ERRORCHECK();
  glUnmapBuffer(GL_ARRAY_BUFFER);
  GL_ERRORCHECK();
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_ERRORCHECK();
  VBuf.Unlock();

  ClearVao();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseVB(VertexBufferBase& VBuf) {
  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

  if (hBuf) {
    delete hBuf;
    VBuf.SetHandle(nullptr);
  }
  
}

///////////////////////////////////////////////////////////////////////////////
enum class AttrType : uint32_t {
  FLOAT = 0,
  FLOAT_NORMALIZED,
  INTEGER,
};

struct vtx_config {
  const std::string mSemantic;
  const int mNu_components;
  const GLenum mType;
  const AttrType _attrtype;
  const int mOffset;
  const glslfx::Pass* mPass;
  glslfx::Attribute* mAttr;

  uint32_t bind_to_attr(const glslfx::Pass* pfxpass, int istride) {
    uint32_t rval = 0;
    if (mPass != pfxpass) {
      const auto& it = pfxpass->_vtxAttributesBySemantic.find(mSemantic);
      mAttr          = nullptr;
      if (it != pfxpass->_vtxAttributesBySemantic.end()) {
        // printf( "gbi::bind_attr pass<%p> attr_sem<%s> istride<%d> found!\n", pfxpass, mSemantic.c_str(), istride );
        mAttr = it->second;
      } else {
        // printf( "gbi::bind_attr pass<%p> attr_sem<%s> istride<%d> NOT found!\n", pfxpass, mSemantic.c_str(), istride );
        // assert(false);
      }
      mPass = pfxpass;
    }
    if (mAttr) {
      // printf( "gbi::bind_attr istride<%d> loc<%d> numc<%d> offs<%d>\n", istride, mAttr->mLocation, mNu_components, mOffset );
      switch( _attrtype ){
        case AttrType::FLOAT:
          glVertexAttribPointer(mAttr->mLocation, mNu_components, mType, false, istride, (void*)(uint64_t)mOffset);
          break;
        case AttrType::FLOAT_NORMALIZED:
          glVertexAttribPointer(mAttr->mLocation, mNu_components, mType, true, istride, (void*)(uint64_t)mOffset);
          break;
        case AttrType::INTEGER:
          glVertexAttribIPointer(mAttr->mLocation, mNu_components, mType, istride, (void*)(uint64_t)mOffset);
          break;
      }
      rval = 1 << mAttr->mLocation;
    } else {
      // printf( "gbi::bind_attr no_attr\n" );
    }
    return rval;
  }
  static void enable_attrs(const uint32_t cmask) {
    for (int iloc = 0; iloc <= 15; iloc++) {
      uint32_t tmask = 1 << iloc;

      bool bena = (tmask & cmask);

      if (bena) {
        // printf("gbi::enable_attrs iloc<%d> : %d\n", iloc, int(bena));
        glEnableVertexAttribArray(iloc);
      } else
        glDisableVertexAttribArray(iloc);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////

static bool EnableVtxBufComponents(const VertexBufferBase& VBuf, const svarp_t priv_data) {
  // printf( "EnableVtxBufComponents\n");
  bool rval = false;
  //////////////////////////////////////////////
  auto pfxpass = priv_data.get<const glslfx::Pass*>();
//////////////////////////////////////////////
#if defined(WIN32)
  const GLenum kGLVTXCOLS = GL_BGRA;
#else
  const GLenum kGLVTXCOLS = 4;
#endif
  //////////////////////////////////////////////
  //////////////////////////////////////////////
  EVtxStreamFormat eStrFmt = VBuf.GetStreamFormat();
  int iStride              = VBuf.GetVtxSize();
  //////////////////////////////////////////////
  GL_ERRORCHECK();
  //////////////////////////////////////////////
  auto _setConfig = [&](auto& configarray) {
    uint32_t component_mask = 0;
    for (vtx_config& vcfg : configarray)
      component_mask |= vcfg.bind_to_attr(pfxpass, iStride);
    rval = true;
    vtx_config::enable_attrs(component_mask);
  };
  //////////////////////////////////////////////
  switch (eStrFmt) {
    case lev2::EVtxStreamFormat::V12: {
      static vtx_config cfgs[] = {{"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0}};
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12C4: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"COLOR0", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12T8: {
      static vtx_config cfgs[] = {{"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0}, {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 12, 0, 0}};
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12T16: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"TEXCOORD0", 4, GL_FLOAT, AttrType::FLOAT, 24, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12B12T16: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"BINORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 24, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 36, 0, 0},
          {"TEXCOORD1", 2, GL_FLOAT, AttrType::FLOAT, 44, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12T8DF12C4: {
      static vtx_config cfgs[] = {
          {"POSITION",  3, GL_FLOAT, AttrType::FLOAT,         0, 0, 0},
          {"NORMAL",    3, GL_FLOAT, AttrType::FLOAT_NORMALIZED,         12, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT,        24, 0, 0},
          {"TEXCOORD1", 3, GL_FLOAT, AttrType::FLOAT,        32, 0, 0},
          {"COLOR0",    4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 44, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12T8DU12C4: {
      static vtx_config cfgs[] = {
          {"POSITION",  3, GL_FLOAT, AttrType::FLOAT,         0, 0, 0},
          {"NORMAL",    3, GL_FLOAT, AttrType::FLOAT_NORMALIZED,         12, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT,        24, 0, 0},
          {"TEXCOORD1", 3, GL_UNSIGNED_INT, AttrType::INTEGER, 32, 0, 0},
          {"COLOR0",    4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 44, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12B12T8I4W4: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"BINORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 24, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 36, 0, 0},
          {"BONEINDICES", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT, 44, 0, 0},
          {"BONEWEIGHTS", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 48, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12T8I4W4: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 24, 0, 0},
          {"BONEINDICES", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 32, 0, 0},
          {"BONEWEIGHTS", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 36, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case EVtxStreamFormat::V12C4N6I2T8: {
      break;
    }
    case EVtxStreamFormat::V12I4N12T8: {
      break;
    }
    case EVtxStreamFormat::V12N12B12T8C4: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT, 12, 0, 0},
          {"BINORMAL", 3, GL_FLOAT, AttrType::FLOAT, 24, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 36, 0, 0},
          {"COLOR0", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 44, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12T16C4: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"NORMAL", 3, GL_FLOAT, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 24, 0, 0},
          {"TEXCOORD1", 2, GL_FLOAT, AttrType::FLOAT, 32, 0, 0},
          {"COLOR0", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 40, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case EVtxStreamFormat::V12C4T16: {
      static vtx_config cfgs[] = {
          {"POSITION", 3, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"COLOR0", 4, GL_UNSIGNED_BYTE, AttrType::FLOAT_NORMALIZED, 12, 0, 0},
          {"TEXCOORD0", 2, GL_FLOAT, AttrType::FLOAT, 16, 0, 0},
          {"TEXCOORD1", 2, GL_FLOAT, AttrType::FLOAT, 24, 0, 0}};
      _setConfig(cfgs);
      break;
    }
    case EVtxStreamFormat::V16T16C16: {
      static vtx_config cfgs[] = {
          {"POSITION", 4, GL_FLOAT, AttrType::FLOAT, 0, 0, 0},
          {"TEXCOORD0", 4, GL_FLOAT, AttrType::FLOAT, 16, 0, 0},
          {"COLOR0", 4, GL_FLOAT, AttrType::FLOAT, 32, 0, 0},
      };
      _setConfig(cfgs);
      break;
    }
    case EVtxStreamFormat::V4C4: {
      rval = false;
      break;
    }
    case EVtxStreamFormat::V4T4: {
      rval = false;
      break;
    }
    case EVtxStreamFormat::V4T4C4: {
      rval = false;
      break;
    }
    default: {
      OrkAssert(false);
      break;
    }
  }
  GL_ERRORCHECK();
  //////////////////////////////////////////////
  if (false == rval) {
    printf("unhandled vtxfmt<%d>\n", int(eStrFmt));
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool GlGeometryBufferInterface::BindVertexStreamSource(const VertexBufferBase& VBuf) {
  svarp_t evb_priv;
  ////////////////////////////////////////////////////////////////////
  glslfx::Interface* pFXI     = static_cast<glslfx::Interface*>(mTargetGL.FXI());
  const glslfx::Pass* pfxpass = pFXI->GetActiveEffect()->_activePass;
  OrkAssert(pfxpass != nullptr);
  evb_priv.set<const glslfx::Pass*>(pfxpass);
  ////////////////////////////////////////////////////////////////////
  // setup VBO or DL
  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
  OrkAssert(hBuf);
  GL_ERRORCHECK();

  void* plat_h = (void*)mTargetGL.GetPlatformHandle();
  auto vao_key = (void*)pfxpass;

  GLVaoHandle* vao_obj = hBuf->BindVao(plat_h, vao_key);

  bool rval = true;

  if (vao_obj->mInited == false) {
    vao_obj->mInited = true;
    glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
    // printf( "VBO<%d>\n", int(hBuf->mVBO) );
    GL_ERRORCHECK();
    //////////////////////////////////////////////
    rval = EnableVtxBufComponents(VBuf, evb_priv);
    //////////////////////////////////////////////
    hBuf->mbSetupSource = true;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool GlGeometryBufferInterface::BindStreamSources(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf) {
  bool rval = false;

  ////////////////////////////////////////////////////////////////////

  svarp_t evb_priv;

  glslfx::Interface* pFXI     = static_cast<glslfx::Interface*>(mTargetGL.FXI());
  const glslfx::Pass* pfxpass = pFXI->GetActiveEffect()->_activePass;
  OrkAssert(pfxpass != nullptr);
  evb_priv.set<const glslfx::Pass*>(pfxpass);

  ////////////////////////////////////////////////////////////////////

  GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
  OrkAssert(hBuf);
  GL_ERRORCHECK();

  void* plat_h = mTargetGL.GetPlatformHandle();

  const auto ph = (const GLIdxBufHandle*)IdxBuf.GetHandle();
  OrkAssert(ph != 0);

  size_t k1    = size_t(ph);
  size_t k2    = size_t(pfxpass);
  auto vao_key = (void*)(k1 xor k2);

  GLVaoHandle* vao_container = hBuf->BindVao(plat_h, vao_key);

  // printf( "vao_container<%p> ibo<%p>\n", vao_container, vao_container->mIBO );

  assert(vao_container != nullptr);

  if (nullptr == vao_container->mIBO) {
    glBindBuffer(GL_ARRAY_BUFFER, hBuf->mVBO);
    // printf( "VBO<%d>\n", int(hBuf->mVBO) );
    GL_ERRORCHECK();
    //////////////////////////////////////////////
    rval = EnableVtxBufComponents(VBuf, evb_priv);
    GL_ERRORCHECK();
    //////////////////////////////////////////////
    assert(ph->mIBO != 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ph->mIBO);
    vao_container->mIBO = ph;
    GL_ERRORCHECK();
  } else {
    rval = true;
    // GLuint ibo = it->second;
    // glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo );
  }

  //#error assign idxbuf to VAO here
  //////////////////////////////////////////////
  hBuf->mbSetupSource = true;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawPrimitiveEML(const VertexBufferBase& VBuf, PrimitiveType eType, int ivbase, int ivcount) {
  ////////////////////////////////////////////////////////////////////
  GL_ERRORCHECK();
  bool bOK = BindVertexStreamSource(VBuf);
  if (false == bOK){
    OrkAssert(false);
    return;
  }
  ////////////////////////////////////////////////////////////////////

  // glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

  int inum = (ivcount == 0) ? VBuf.GetNumVertices() : ivcount;

  if (inum) {
    GL_ERRORCHECK();
    switch (eType) {
      case PrimitiveType::LINES: { // orkprintf( "drawarrays: <ivbase %d> <inum %d> lines\n", ivbase, inum );
        glDrawArrays(GL_LINES, ivbase, inum);
        break;
      }
      case PrimitiveType::QUADS:
        // orkprintf( "drawarrays: %d quads\n", inum );
        // GL_ERRORCHECK();
        // glDrawArrays( GL_QUADS, ivbase, inum );
        // GL_ERRORCHECK();
        // miTrianglesRendered += (inum/2);
        break;
      case PrimitiveType::TRIANGLES:
        glDrawArrays(GL_TRIANGLES, ivbase, inum);
        miTrianglesRendered += (inum / 3);
        break;
      case PrimitiveType::TRIANGLESTRIP:
        // printf( "drawarrays: <ivbase %d> <inum %d> tristrip\n", ivbase, inum );
        glDrawArrays(GL_TRIANGLE_STRIP, ivbase, inum);
        miTrianglesRendered += (inum - 2);
        break;
      case PrimitiveType::POINTS:
        GL_ERRORCHECK();
        //glPointSize(1.0f);
        glEnable(GL_PROGRAM_POINT_SIZE);
        //printf("drawpoints<%d,%d>\n", ivbase, inum);
        glDrawArrays(GL_POINTS, ivbase, inum);
        GL_ERRORCHECK();
        break;
      case PrimitiveType::PATCHES:
        GL_ERRORCHECK();
        glPointSize(32.0f);
#if defined(ORK_OSX)
        glPatchParameteri(GL_PATCH_VERTICES, 3);
#else
        // extern PFNGLPATCHPARAMETERIPROC GLPPI;
        // GLPPI(GL_PATCH_VERTICES, 3);
#endif
        glDrawArrays(GL_PATCHES, ivbase, inum);
        GL_ERRORCHECK();
        break;
        /*			case PrimitiveType::POINTSPRITES:
                        GL_ERRORCHECK();
                        glPointSize( mTargetGL.currentMaterial()->mfParticleSize );

                        glEnable( GL_POINT_SPRITE_ARB );
                        glDrawArrays( GL_POINTS, 0, iNum );
                        glDisable( GL_POINT_SPRITE_ARB );

                        GL_ERRORCHECK();
                        break;
                    default:
                        glDrawArrays( GL_POINTS, 0, iNum );
                        //OrkAssert( false );
                        break;*/
      default:
        OrkAssert(false);
        break;
    }
    GL_ERRORCHECK();
  }
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)

void GlGeometryBufferInterface::DrawPrimitiveEML(
    const FxShaderStorageBuffer* SSBO, //
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  auto ssb     = SSBO->_impl.get<glslfx::ShaderStorageBuffer*>();
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);

  if (ivcount) {
    GL_ERRORCHECK();
    switch (eType) {
      case PrimitiveType::LINES: { 
        glDrawArrays(GL_LINES, ivbase, ivcount );
        break;
      }
      case PrimitiveType::TRIANGLES: { 
        glDrawArrays(GL_TRIANGLES, ivbase, ivcount );
        break;
      }
      case PrimitiveType::TRIANGLESTRIP: { 
        glDrawArrays(GL_TRIANGLE_STRIP, ivbase, ivcount );
        break;
      }
      case PrimitiveType::POINTS: { 
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDrawArrays(GL_POINTS, ivbase, ivcount );
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
}

#endif
///////////////////////////////////////////////////////////////////////////////
// epass thru

void GlGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  GL_ERRORCHECK();
  ////////////////////////////////////////////////////////////////////

  BindStreamSources(VBuf, IdxBuf);

  int iNum = IdxBuf.GetNumIndices();

  auto plat_handle = static_cast<const GLIdxBufHandle*>(IdxBuf.GetHandle());

  int imin = plat_handle->mMinIndex;
  int imax = plat_handle->mMaxIndex;

  // GLint maxidx = 0;
  // glGetIntegerv( GL_MAX_ELEMENTS_INDICES, & maxidx );
  // printf( "iminidx<%d> maxidx<%d> iNum<%d>\n", imin, imax, iNum );

  // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  GLenum glprimtype = 0;
  if (iNum) {
    GL_ERRORCHECK();
    switch (eType) {
      case PrimitiveType::LINES: { // orkprintf( "drawarrays: %d lines\n", iNum );
        glprimtype = GL_LINES;
        break;
      }
      case PrimitiveType::TRIANGLES:
        printf( "drawindexedtris inum<%d> imin<%d> imax<%d>\n", iNum/3, imin, imax );
        glprimtype = GL_TRIANGLES;
        miTrianglesRendered += (iNum / 3);
        break;
      case PrimitiveType::TRIANGLESTRIP:
        printf( "drawindexedtristrip inum<%d>\n", iNum-2 );
        glprimtype = GL_TRIANGLE_STRIP;
        miTrianglesRendered += (iNum - 2);
        break;
      case PrimitiveType::POINTS:
        glprimtype = GL_POINTS;
        break;
      default:
        OrkAssert(false);
        break;
    }
    if (glprimtype != 0) {
      if (ivbase != 0)
        glDrawElementsBaseVertex(glprimtype, iNum, GL_UNSIGNED_SHORT, nullptr, ivbase);
      else
        glDrawRangeElements(glprimtype, imin, imax, iNum, GL_UNSIGNED_SHORT, nullptr);
    }
    GL_ERRORCHECK();
  }
  GL_ERRORCHECK();
  // glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    size_t instance_count) {
  GL_ERRORCHECK();
  ////////////////////////////////////////////////////////////////////
  BindStreamSources(VBuf, IdxBuf);
  int iNum          = IdxBuf.GetNumIndices();
  auto plat_handle  = static_cast<const GLIdxBufHandle*>(IdxBuf.GetHandle());
  int imin          = plat_handle->mMinIndex;
  int imax          = plat_handle->mMaxIndex;
  GLenum glprimtype = 0;
  if (iNum) {
    GL_ERRORCHECK();
    switch (eType) {
      case PrimitiveType::LINES: { // orkprintf( "drawarrays: %d lines\n", iNum );
        glprimtype = GL_LINES;
        break;
      }
      case PrimitiveType::TRIANGLES:
        // printf( "drawindexedtris inum<%d> imin<%d> imax<%d>\n", iNum/3, imin, imax );
        glprimtype = GL_TRIANGLES;
        miTrianglesRendered += (iNum / 3) * instance_count;
        break;
      case PrimitiveType::TRIANGLESTRIP:
        // printf( "drawindexedtristrip inum<%d>\n", iNum-2 );
        glprimtype = GL_TRIANGLE_STRIP;
        miTrianglesRendered += (iNum - 2) * instance_count;
        break;
      case PrimitiveType::POINTS:
        glprimtype = GL_POINTS;
        break;
      default:
        OrkAssert(false);
        break;
    }
    if (glprimtype != 0) {
      glDrawElementsInstanced(glprimtype, iNum, GL_UNSIGNED_SHORT, nullptr, instance_count);
    }
    GL_ERRORCHECK();
  }
}

///////////////////////////////////////////////////////////////////////////////

void* GlGeometryBufferInterface::LockIB(IndexBufferBase& IdxBuf, int ibase, int icount) {
  void* rval = nullptr;
  assert(ibase == 0);

  if (icount == 0)
    icount = IdxBuf.GetNumIndices();

  if (nullptr == IdxBuf.GetHandle()) {

    GLIdxBufHandle* plat_handle = new GLIdxBufHandle;

    auto p16                 = new U16[icount];
    plat_handle->mBuffer     = p16;
    rval                     = (void*)p16;
    plat_handle->mNumIndices = icount;

    IdxBuf.SetHandle((void*)plat_handle);

  } else {
    assert(false);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB(IndexBufferBase& IdxBuf) {
  auto plat_handle = (GLIdxBufHandle*)IdxBuf.GetHandle();
  assert(plat_handle != nullptr);
  {
    const void* src_data = plat_handle->mBuffer;
    int iblen            = plat_handle->mNumIndices * sizeof(U16);

    // printf( "UNLOCKIBO\n");
    glGenBuffers(1, (GLuint*)&plat_handle->mIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plat_handle->mIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, iblen, src_data, GL_STATIC_DRAW);

    auto p16 = static_cast<const uint16_t*>(src_data);

    uint16_t umin = 65535;
    uint16_t umax = 0;
    for (int i = 0; i < plat_handle->mNumIndices; i++) {
      uint16_t u = p16[i];
      if (u > umax)
        umax = u;
      if (u < umin)
        umin = u;
    }
    plat_handle->mMinIndex = int(umin);
    plat_handle->mMaxIndex = int(umax);

    // printf( "umin<%d> umax<%d>\n", int(umin), int(umax) );
    // glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
  }
}

///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockIB(const IndexBufferBase& IdxBuf, int ibase, int icount) {
  auto ph = (GLIdxBufHandle*)IdxBuf.GetHandle();
  assert(ph != nullptr);
  const void* rval = ph->mBuffer;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB(const IndexBufferBase& IdxBuf) {
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseIB(IndexBufferBase& IdxBuf) {
  auto plat_handle = (GLIdxBufHandle*)IdxBuf.GetHandle();

  if (plat_handle){

    uint32_t ibo = plat_handle->mIBO;
    if(ibo){
      glDeleteBuffers(1,&ibo);
    }

    delete plat_handle;
  }

  IdxBuf.SetHandle(0);
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)

void GlGeometryBufferInterface::DrawMeshTasksNV(uint32_t first, uint32_t count) {
  glDrawMeshTasksNV((GLuint)first, (GLuint)count);
}

void GlGeometryBufferInterface::DrawMeshTasksIndirectNV(int32_t* indirect) {
  glDrawMeshTasksIndirectNV((GLintptr)indirect);
}

void GlGeometryBufferInterface::MultiDrawMeshTasksIndirectNV(int32_t* indirect, uint32_t drawcount, uint32_t stride) {
  glMultiDrawMeshTasksIndirectNV((GLintptr)indirect, (GLsizei)drawcount, (GLsizei)stride);
}

void GlGeometryBufferInterface::MultiDrawMeshTasksIndirectCountNV(
    int32_t* indirect,
    int32_t* drawcount,
    uint32_t maxdrawcount,
    uint32_t stride) {
  glMultiDrawMeshTasksIndirectCountNV((GLintptr)indirect, (GLintptr)drawcount, (GLsizei)maxdrawcount, (GLsizei)stride);
}

#endif
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
