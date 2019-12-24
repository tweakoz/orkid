////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/filter/filter.h>
#include <ork/gfx/dds.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

bool VolTexAssemble(const tokenlist& toklist) {
  /*
  ork::tool::FilterOptMap OptionsMap;
  OptionsMap.SetDefault("--in", "yo.vtc");
  OptionsMap.SetDefault("--out", "yo.dds");
  OptionsMap.SetOptions(toklist);

  std::string tex_in  = OptionsMap.GetOption("--in")->GetValue();
  std::string tex_out = OptionsMap.GetOption("--out")->GetValue();

  printf("IN<%s>\n", tex_in.c_str());
  printf("OUT<%s>\n", tex_out.c_str());

  file::Path VtcPath(tex_in.c_str());
  file::Path DdsPath(tex_out.c_str());

  bool bVtcPresent = FileEnv::GetRef().DoesFileExist(VtcPath);

  if (false == bVtcPresent)
    return false;

  ork::File fil(VtcPath, EFM_READ);
  void* pdata = 0;
  size_t ilength;
  EFileErrCode ecode = fil.Load(&pdata, ilength);
  printf("length<%lu>\n", ilength);

  orkvector<std::string> splitvect;
  SplitString((const char*)pdata, splitvect, "\n ");

  int inumitems = splitvect.size();
  std::vector<ork::dds::DDFile*> ddsfiles;
  printf("NUMITEMS<%d>\n", inumitems);
  for (int i = 0; i < inumitems; i++) {
    const std::string& item = splitvect[i];
    printf("item<%d:%s>\n", i, item.c_str());
    ddsfiles.push_back(new dds::DDFile(item.c_str()));
  }
  size_t idepth             = nextPowerOfTwo(inumitems);
  dds::DDS_HEADER OutHeader = ddsfiles[0]->mHeader;
  OutHeader.dwDepth         = idepth;

  ork::File outf(DdsPath, EFM_WRITE);
  int ireadctr = 0;
  int iw       = OutHeader.dwWidth;
  int ih       = OutHeader.dwHeight;
  int id       = idepth;
  /////////////////////////////////////
  int iL                  = (iw < ih) ? iw : ih;
  iL                      = (iL < id) ? iL : id;
  int imipcnt             = powerOfTwoIndex(iL) + 1;
  OutHeader.dwMipMapCount = imipcnt;
  outf.Write(&OutHeader, sizeof(OutHeader));
  bool bBLOCKS = ddsfiles[0]->HasBlocks();
  /////////////////////////////////////
  int bytesperblock = ddsfiles[0]->mLoadInfo.blockBytes;
  for (int imip = 0; imip < imipcnt; imip++) {
    int iBwidth  = bBLOCKS ? (iw + 3) / 4 : iw;
    int iBheight = bBLOCKS ? (ih + 3) / 4 : ih;
    int iImgsize = (iBwidth * iBheight) * bytesperblock;
    for (int i = 0; i < idepth; i++) {
      int j                      = (i < inumitems) ? i : (inumitems - 1);
      const dds::DDFile* ddsfile = ddsfiles[j];
      const char* imgdata        = ddsfile->mpImageDataBase + ireadctr;
      outf.Write(imgdata, iImgsize);
    }
    ireadctr += iImgsize;
    iw >>= 1;
    ih >>= 1;
  }
  outf.Close();
  // std::string tex_pla = OptionsMap.GetOption( "--out" );
  /////////////////////////////////////////////
  // ork::file::Path InPath( tex_in.c_str() );
  */
  return true;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::tool
