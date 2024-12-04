////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/primitives_points.inl>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::primitives {
///////////////////////////////////////////////////////////////////////////////

PointsData::PointsData( datablock_ptr_t db, 
                        int num_points, 
                        EVtxStreamFormat format) {

  size_t blocksize = 0;
  switch(format){
    case EVtxStreamFormat::V12C4: {
      blocksize = sizeof(VtxV12C4)*num_points;
      break;
    }
    case EVtxStreamFormat::V12T8: {
      blocksize = sizeof(VtxV12T8)*num_points;
      break;
    }
    default:
      OrkAssert(false);
      break;
  }

  if(db){
    OrkAssert(db->length() == blocksize);
    _datablock = db;
  }
  else {
    _datablock = std::make_shared<DataBlock>();
    _datablock->allocateBlock(blocksize);
  }                          


  _num_points = num_points;
  _format = format;
}

///////////////////////////////////////////////////////////////////////////////

void PointsData::transformInPlace(const fmtx4& mtx) {
  switch(_format){
    case EVtxStreamFormat::V12C4: {
      auto p_v12c4 = (VtxV12C4*) _datablock->data();
      for(int i=0; i<_num_points; i++){
        auto& vtx = p_v12c4[i];
        fvec4 pos(vtx.x,vtx.y,vtx.z,1.0f);
        pos = pos.transform(mtx);
        vtx.x = pos.x;
        vtx.y = pos.y;
        vtx.z = pos.z;
      }
      break;
    }
    case EVtxStreamFormat::V12T8: {
      auto p_v12t8 = (VtxV12T8*) _datablock->data(); 
      for(int i=0; i<_num_points; i++){
        auto& vtx = p_v12t8[i];
        fvec4 pos(vtx.pos,1.0f);
        pos = pos.transform(mtx);
        vtx.pos = pos.xyz();
      }
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

pointsdata_ptr_t PointsData::transformed(const fmtx4& mtx) const {
  auto rval = std::make_shared<PointsData>(nullptr,_num_points,_format);
  auto dblock_dest = rval->_datablock;
  auto dest = (void*) dblock_dest->data();
  memcpy_fast(dest,_datablock->data(),_datablock->length());
  rval->transformInPlace(mtx);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

pointsdata_ptr_t PointsData::convertToV12C4(image_ptr_t image) const {
  switch(_format){
    case EVtxStreamFormat::V12T8: {
      auto rval = std::make_shared<PointsData>(nullptr,_num_points,EVtxStreamFormat::V12C4);
      auto dblock_dest = rval->_datablock;
      auto dest = dblock_dest->data();
      auto dest_typed = (VtxV12C4*)dest;
      auto src_typed = (VtxV12T8*)_datablock->data();

      for(int i=0; i<_num_points; i++){
        auto& src = src_typed[i];
        auto& dst = dest_typed[i];
        dst.x = src.pos.x;
        dst.y = src.pos.y;
        dst.z = src.pos.z;
        float u = src.uv0.x;
        float v = src.uv0.y;
        if(image){
          int iu = int(u*float(image->_width-1));
          int iv = int(v*float(image->_height-1));
          const uint8_t* pixel = image->pixel8(iu,iv);
          // todo sample image
          uint32_t r = uint32_t(pixel[0]);
          uint32_t g = uint32_t(pixel[1]);
          uint32_t b = uint32_t(pixel[2]);
          dst.color = r<<8|(g<<16)|(b<<24);
        }
        else {
          dst.color = 0xffffffff;
        }
      }
      return rval;
    }
  }
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
