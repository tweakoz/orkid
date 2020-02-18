///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <OpenImageIO/imageio.h>
#include <ork/file/file.h>
#include <ork/pch.h>
#include <ork/util/crc64.h>
#include <ork/lev2/gfx/terrain/heightmap.h>

OIIO_NAMESPACE_USING

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

HeightMap HeightMap::gdefhm(3, 3);
typedef orkmap<float, fvec4> gradient_t;

///////////////////////////////////////////////////////////////////////////////
HeightMap::HeightMap(int isx, int isz)
    : miGridSizeX(0)
    , miGridSizeZ(0)
    , mHeightData()
    , mMin(std::numeric_limits<float>::max())
    , mMax(-std::numeric_limits<float>::max())
    , mRange(0.0f)
    , mMutex("shmMutex")
    , mfWorldSizeX(1.0f)
    , mfWorldSizeZ(1.0f)
    , mIndexToUnitX(1.0f)
    , mIndexToUnitZ(1.0f)
    , mWorldHeight(1.0f)

{
  SetGridSize(isx, isz);
}
void HeightMap::ResetMinMax() {
  mMin   = std::numeric_limits<float>::max();
  mMax   = -std::numeric_limits<float>::max();
  mRange = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
void HeightMap::SetGridSize(int iw, int ih) {
  mMutex.Lock();
  {
    ResetMinMax();
    miGridSizeX = iw;
    miGridSizeZ = ih;
    mHeightData.clear();
    mHeightData.resize(iw * ih);
    float fiiw(1.0f / float(iw));
    float fiih(1.0f / float(ih));
    mIndexToUnitX = fiiw;
    mIndexToUnitZ = fiih;
  }
  mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
HeightMap::~HeightMap() {
  if (_pu16) {
    delete[] _pu16;
  }
}
///////////////////////////////////////////////////////////////////////////////
void HeightMap::SetHeight(int ix, int iz, float fh) {
  int iaddr          = CalcAddress(ix, iz);
  mHeightData[iaddr] = fh;
  if (fh > mMax)
    mMax = fh;
  if (fh < mMin)
    mMin = fh;
  mRange = (mMax - mMin);
}
///////////////////////////////////////////////////////////////////////////////
float HeightMap::GetHeight(int ix, int iz) const {
  int iaddr = CalcAddress(ix, iz);
  return mHeightData[iaddr];
}

///////////////////////////////////////////////////////////////////////////////

fvec3 HeightMap::XYZ(int iX, int iZ) const {
  int isx = GetGridSizeX();
  int isz = GetGridSizeZ();

  // float fminy = GetMinHeight();
  // float frngy = GetHeightRange();

  int ixd2 = isx >> 1;
  int izd2 = isz >> 1;

  float fx = (float(iX - ixd2) * mIndexToUnitX) * mfWorldSizeX;
  float fz = (float(iZ - izd2) * mIndexToUnitZ) * mfWorldSizeZ;

  float fy  = GetHeight(iX, iZ) * mWorldHeight;
  auto rval = fvec3(fx, fy, fz);
  // printf( "XYZ<%d %d> - <%f %f %f>\n", iX, iZ, fx, fy, fz );
  return rval;
}

fvec3 HeightMap::ComputeNormal(int ix1, int iz1) const {
  int isx = GetGridSizeX();
  int isz = GetGridSizeZ();

  int ix0 = (ix1 - 1);
  if (ix0 < 0)
    ix0 = 0;
  int iz0 = (iz1 - 1);
  if (iz0 < 0)
    iz0 = 0;

  int ix2 = (ix1 + 1);
  if (ix2 > (isx - 1))
    ix2 = isx - 1;
  int iz2 = (iz1 + 1);
  if (iz2 > (isz - 1))
    iz2 = isz - 1;

  // fvec3 hscale(1.0f,mWorldHeight,1.0f);

  fvec3 VC = XYZ(ix1, iz1);

  fvec3 d0 = (XYZ(ix0, iz0) - VC).Normal();
  fvec3 d1 = (XYZ(ix1, iz0) - VC).Normal();
  fvec3 d2 = (XYZ(ix2, iz0) - VC).Normal();
  fvec3 d3 = (XYZ(ix2, iz1) - VC).Normal();
  fvec3 d4 = (XYZ(ix2, iz2) - VC).Normal();
  fvec3 d5 = (XYZ(ix1, iz2) - VC).Normal();
  fvec3 d6 = (XYZ(ix0, iz2) - VC).Normal();
  fvec3 d7 = (XYZ(ix0, iz1) - VC).Normal();

  fvec3 c0 = d0.Cross(d1);
  fvec3 c1 = d1.Cross(d2);
  fvec3 c2 = d2.Cross(d3);
  fvec3 c3 = d3.Cross(d4);
  fvec3 c4 = d4.Cross(d5);
  fvec3 c5 = d5.Cross(d6);
  fvec3 c6 = d6.Cross(d7);
  fvec3 c7 = d7.Cross(d0);

  fvec3 vdx = -(c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7).Normal();

  return vdx;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 HeightMap::Min() const {
  fvec3 ret(mfWorldSizeX * -0.5f, mMin, mfWorldSizeZ * -0.5f);
  return ret;
}
fvec3 HeightMap::Max() const {
  fvec3 ret(mfWorldSizeX * 0.5f, mMax, mfWorldSizeZ * 0.5f);
  return ret;
}
fvec3 HeightMap::Range() const {
  fvec3 ret(mfWorldSizeX, mRange, mfWorldSizeZ);
  return ret;
}

bool HeightMap::Load(const ork::file::Path& pth) {
  auto abs_path = pth.ToAbsolute();

  bool bexists = FileEnv::DoesFileExist(abs_path);

  uint16_t hfmin = 0xffff;
  uint16_t hfmax = 0x0;

  ork::Timer timer;
  timer.Start();

  boost::Crc64 hasher;

  if (bexists) {
    auto oiio_img = ImageInput::create(abs_path.c_str());
    if (!oiio_img)
      return false;
    ImageSpec spec;
    oiio_img->open(abs_path.c_str(), spec);
    int xres         = spec.width;
    int yres         = spec.height;
    int num_channels = spec.nchannels;
    _pu16            = new uint16_t[xres * yres * num_channels];

    printf("xres<%d> yres<%d> numch<%d>\n", xres, yres, num_channels);

    bool read_ok = oiio_img->read_image(TypeDesc::UINT16, _pu16);
    oiio_img->close();

    OrkAssert(read_ok);

    int inumpix = (xres * yres);

    hasher.accumulate(_pu16, xres * yres * num_channels * sizeof(uint16_t));
    hasher.finish();
    _hash = hasher.result();

    //////////////////////////////////////////
    // fill in heightmap
    //////////////////////////////////////////

    ork::EndianContext ectx;
    ectx.mendian = ork::EENDIAN_LITTLE;

    U16 umin = 0xffff;
    U16 umax = 0x0000;
    for (int ip = 0; ip < inumpix; ip++) {
      U16 upix = _pu16[ip];
      ork::swapbytes_dynamic<U16>(upix);
      if (upix > umax)
        umax = upix;
      if (upix < umin)
        umin = upix;
    }
    U16 urange = (umax - umin);
    urange     = (urange == 0) ? 1 : urange;

    this->SetGridSize(xres, yres);

    this->GetLock().Lock();
    {

      for (int iz = 0; iz < yres; iz++) {

        for (int ix = 0; ix < xres; ix++) {

          int iaddr = (iz * xres) + ix;

          U16 upix = (_pu16[iaddr] - umin);

          if (upix < hfmin)
            hfmin = upix;
          if (upix > hfmax)
            hfmax = upix;
          ork::swapbytes_dynamic<U16>(upix);

          float fh = (float(upix) / float(urange)) - 0.5f;

          this->SetHeight(ix, iz, fh);
        }
      }
    }
    this->GetLock().UnLock();
  }
  printf("HeightMap<%p> hfmin<%d> hfmax<%d>\n", this, int(hfmin), int(hfmax));
  printf("HeightMap<%p> timer<%g> abs_path<%s> exists<%d>\n", this, timer.SecsSinceStart(), abs_path.c_str(), int(bexists));
  return bexists;
}

static fvec4 GetGradientColor(float fin, const gradient_t& gmap) {

  fvec4 rval = fcolor4::White();

  if (gmap.size() >= 2) {
    fin = 1.0f - fin;

    if (fin < 0.0f)
      fin = 0.0f;
    if (fin > 1.0f)
      fin = 1.0f;

    auto itu = gmap.lower_bound(fin);

    if (itu == gmap.end()) {
      itu--;
      rval = itu->second;
    } else {
      auto itl = itu;
      itl--;

      if (itl != gmap.end()) {
        float frange = itu->first - itl->first;
        float flerp  = (fin - itl->first) / frange;

        rval.Lerp(itl->second, itu->second, flerp);
      }
    }
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
fvec4 GradientSet::Lerp(float fu, float fv) const {
  fvec3 lo = GetGradientColor(fu, *mGradientLo);
  fvec3 hi = GetGradientColor(fu, *mGradientHi);
  fvec3 result;
  result.Lerp(lo, hi, fv);
  return result;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
