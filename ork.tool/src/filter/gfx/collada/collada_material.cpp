////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/application/application.h>
#if defined(USE_FCOLLADA)
#include <ork/kernel/prop.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/collada/daeutil.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::tool {
bool NvttCompress(const ork::tool::FilterOptMap& options);
}

namespace ork::tool::meshutil {

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::ParseMaterialBindings(void) {
  return ParseColladaMaterialBindings(*mDocument, mMaterialSemanticBindingMap);
}

///////////////////////////////////////////////////////////////////////////////

collada_material_info_ptr_t CColladaModel::GetMaterialFromShadingGroup(const std::string& ShadingGroupName) const {
  static collada_material_info_ptr_t kDefaultMaterial = std::make_shared<ColladaMaterialInfo>();

  auto itsh = mMaterialSemanticBindingMap.find(ShadingGroupName);

  if (mMaterialSemanticBindingMap.end() != itsh) {
    const std::string& MaterialName = itsh->second.mMaterialDaeId;

    auto itmat = mMaterialMap.find(ShadingGroupName);

    if (mMaterialMap.end() != itmat) {
      return itmat->second;
    }
  }
  return kDefaultMaterial;
}

///////////////////////////////////////////////////////////////////////////////

struct StandardEffectTexGetter {

  static void GetImgData(const FCDImage* pimg, std::string& TexFileName) {
    TexFileName = pimg->GetFilename();
  }
  static void GetTexData(const FCDTexture* ptex, std::string& TexFileName, float& RepeatU, float& RepeatV) {
    const FCDImage* Image = ptex->GetImage();

    GetImgData(Image, TexFileName);

    const FCDExtra* TexExtra        = ptex->GetExtra();
    const FCDExtra* ImageExtra      = Image->GetExtra();
    const FCDETechnique* TexMayaTek = TexExtra->GetDefaultType()->FindTechnique("MAYA");
    const FCDETechnique* ImgMayaTek = ImageExtra->GetDefaultType()->FindTechnique("MAYA");

    printf("TEXNAME<%s>\n", TexFileName.c_str());

    ///////////////////////////////////////////////////////////
    // check image name (or image name base)
    ///////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////
    // check for image sequence
    ///////////////////////////////////////////////////////////
    if (ImgMayaTek) {
      const FCDENode* ImgSeqNode = ImgMayaTek->FindChildNode("image_sequence");

      int imgseq = 0;

      if (ImgSeqNode) {
        const fchar* ImgSeqVal = ImgSeqNode->GetContent();

        imgseq = atoi(ImgSeqVal);

        if (imgseq) {
          orkprintf("flagged as image sequence!\n");

          file::Path ImgBasePath(TexFileName.c_str());

          std::string ext = ImgBasePath.GetExtension().c_str();

          ImgBasePath.SetExtension("");

          std::string base_index = ImgBasePath.GetExtension().c_str();

          ImgBasePath.SetExtension("");

          int ibidx = atoi(base_index.c_str());

          std::string BaseName = ImgBasePath.GetName().c_str();

          ImgBasePath.SetFile("");

          std::string wildcard               = CreateFormattedString("%s.*.%s", BaseName.c_str(), ext.c_str());
          orkset<file::Path::NameType> files = FileEnv::filespec_search_sorted(wildcard.c_str(), ImgBasePath.c_str());

          for (orkset<file::Path::NameType>::const_iterator it = files.begin(); it != files.end(); it++) {
            const file::Path::NameType& filename = (*it);

            orkprintf("found sequence image <%s>\n", filename.c_str());
          }
        }
      }
    }

    ///////////////////////////////////////////////////////////
    // check repeat UV
    ///////////////////////////////////////////////////////////

    RepeatU = 1.0f;
    RepeatV = 1.0f;
    if (TexMayaTek) {
      const FCDENode* RepeatUNode = TexMayaTek->FindChildNode("repeatU");
      const FCDENode* RepeatVNode = TexMayaTek->FindChildNode("repeatV");

      const fchar* prpU = RepeatUNode ? RepeatUNode->GetContent() : "1.0f";
      const fchar* prpV = RepeatVNode ? RepeatVNode->GetContent() : "1.0f";

      RepeatU = float(atof(prpU));
      RepeatV = float(atof(prpV));
    }

    ///////////////////////////////////////////////////////////
  }

  static void CheckChannel(FCDEffectStandard* StdProf, uint32 texbucket, int isubtex, ork::meshutil::MaterialChannel& RefMatCh) {
    int TexCount(StdProf->GetTextureCount(texbucket));

    if (isubtex < TexCount) {
      const FCDTexture* ptex = StdProf->GetTexture(texbucket, isubtex);
      GetTexData(ptex, RefMatCh.mTextureName, RefMatCh.mRepeatU, RefMatCh.mRepeatV);
    }
  }

  static void DoIt(FCDEffectStandard* StdProf, ColladaMaterialInfo& ColMat) {
    switch (ColMat.mLightingType) {
      case ork::meshutil::MaterialInfo::ELIGHTING_LAMBERT:
      case ork::meshutil::MaterialInfo::ELIGHTING_BLINN:
      case ork::meshutil::MaterialInfo::ELIGHTING_PHONG:
        CheckChannel(StdProf, FUDaeTextureChannel::DIFFUSE, 0, ColMat.mDiffuseMapChannel);
        CheckChannel(StdProf, FUDaeTextureChannel::SPECULAR, 0, ColMat.mSpecularMapChannel);
        CheckChannel(StdProf, FUDaeTextureChannel::BUMP, 0, ColMat.mNormalMapChannel);
        CheckChannel(StdProf, FUDaeTextureChannel::AMBIENT, 0, ColMat.mAmbientMapChannel);
        break;
    }
  }
};

///////////////////////////////////////////////////////////////////////////////

void ColladaMaterialInfo::ParseStdMaterial(FCDEffectStandard* StdProf) {
  FCDEffectStandard::LightingType lighting_type = StdProf->GetLightingType();

  mSpecularPower = StdProf->GetShininess();
  mSpecularPower = (0.0f == mSpecularPower) ? 256.0f : (10.0f / mSpecularPower);

  printf("ParseStdMaterial\n");
  switch (lighting_type) {
    case FCDEffectStandard::LAMBERT:
      mLightingType = ork::meshutil::MaterialInfo::ELIGHTING_LAMBERT;
      break;
    case FCDEffectStandard::BLINN:
      mLightingType = ork::meshutil::MaterialInfo::ELIGHTING_BLINN;
      break;
    case FCDEffectStandard::PHONG:
      mLightingType = ork::meshutil::MaterialInfo::ELIGHTING_PHONG;
      break;
    default:
      mLightingType = ork::meshutil::MaterialInfo::ELIGHTING_NONE;
      break;
  }

  StandardEffectTexGetter::DoIt(StdProf, *this);

  const FMVector4& EmColor = StdProf->GetEmissionColor();
  mEmissiveColor.SetXYZ(EmColor.x, EmColor.y, EmColor.z);
  mEmissiveColor.SetW(1.0f);

  const FMVector4& TransColor = StdProf->GetTranslucencyColor();
  mTransparencyMode           = StdProf->GetTransparencyMode();
  mTransparencyColor.SetXYZ(TransColor.x, TransColor.y, TransColor.z);
  mTransparencyColor.SetW(TransColor.x);
}

///////////////////////////////////////////////////////////////////////////////

void ColladaMaterialInfo::ParseMaterial(FCDocument* doc, const std::string& ShadingGroupName, const std::string& MaterialName) {

  printf("ParseMaterial\n");

  FCDMaterialLibrary* MatLib = doc->GetMaterialLibrary();
  FCDMaterial* material      = MatLib->FindDaeId(MaterialName.c_str());

  // const fstring& material_note = material->GetNote();
  // std::string smaterial_note = material_note.c_str();
  // printf( "smaterial_note<%s>\n", smaterial_note.c_str() );

  /*u32 itm = smaterial_note.find( "materialobject(" );
  std::string material_object;
  std::string material_namespace;
  if( itm != std::string::npos )
  {
      itm = smaterial_note.find( "(" )+1;
      u32 itm2 = smaterial_note.find( ")" );

      if( itm2 != std::string::npos )
      {
          material_object = smaterial_note.substr( itm, (itm2-itm) );

          itm = material_object.find( ":" );
          if( itm != std::string::npos )
          {
              material_namespace = material_object.substr( 0, itm );
          }


      }

  }*/

  mShadingGroupName = ShadingGroupName;
  mMaterialName     = MaterialName;
  mSpecularPower    = 1.0f;

  printf("mShadingGroupName<%s> MaterialName<%s> material<%p>\n", mShadingGroupName.c_str(), MaterialName.c_str(), material);

  if (material) {
    FCDEffect* Effect = material->GetEffect();

    if (Effect) {
      mFx                        = Effect;
      FCDEffectStandard* StdProf = static_cast<FCDEffectStandard*>(Effect->FindProfile(FUDaeProfileType::COMMON));
      FCDEffectProfileFX* FxProf = static_cast<FCDEffectProfileFX*>(Effect->FindProfile(FUDaeProfileType::CG));
      auto extra                 = Effect->GetExtra();
      printf("extra<%p>\n", extra);

      bool do_fxmtl = false;

      if (FxProf) {
        do_fxmtl = true;
      } else if (extra) {
        auto exx = extra->FindType("import");
        printf("exx<%p>\n", exx);
        if (exx) {
          auto tek = exx->GetTechnique(0);
          printf("tek<%p>\n", tek);
          if (tek) {
            auto profile = tek->GetProfile();
            printf("profile<%s>\n", profile);
            if (0 == strcmp(profile, "NVIDIA_FXCOMPOSER")) {
              do_fxmtl = true;
            }
          }
        }
      }

      if (do_fxmtl) {
        // ParseFxMaterial( material );
        mFxProfile = (FCDMaterial*)material->Clone(0);
      } else if (StdProf) {
        // ParseStdMaterial( StdProf );
        mStdProfile = StdProf;
      }
    }

    if (_orkMaterial) {
      _orkMaterial->SetName(AddPooledString(MaterialName.c_str()));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

ColladaMaterialInfo::ColladaMaterialInfo()
    : ork::meshutil::MaterialInfo()
    , mStdProfile(0) {
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::ConvertTextures(const file::Path& outmdlpth, ork::tool::FilterOptMap& options) {
  bool rv = true;

  ork::file::Path InDir(mFileName.c_str());
  InDir.SetExtension(0);
  InDir.SetFile(0);

  ork::file::Path OutDir = outmdlpth;
  OutDir.SetExtension(0);
  OutDir.SetFile(0);

  for (auto passet : mTextures) {
    auto path = ork::AssetPath(passet->GetName());

    lev2::Texture* ptex = (passet == nullptr) ? nullptr : passet->GetTexture();

    ork::file::Path InPath = path;

    std::string tmpstr(InPath.c_str());

    size_t f = tmpstr.find("data/src/");
    if (f == std::string::npos) {
      orkerrorlog("ERROR: Input texture path is outside of 'data/src/'! (%s)\n", InPath.c_str());
      return false;
    }
    auto base_out_dir = std::string("data/") + options.GetOption("-platform")->GetValue();

    file::Path OutPath = outmdlpth;
    OutPath.SetFile(path.GetName().c_str());
    OutPath.SetExtension("dds");
    options.GetOption("--in")->SetValue(InPath.c_str());
    options.GetOption("--out")->SetValue(OutPath.c_str());

    ork::file::Path::SmallNameType extension = InPath.GetExtension();

    if (strcmp(extension.c_str(), "dds") == 0) {
      printf("OutPath<%s>\n", OutPath.c_str());
      fxstring<1024> cmd_str;
      cmd_str.format("cp %s %s", InPath.c_str(), OutPath.c_str());
      system(cmd_str.c_str());
    } else { // convert via NVTT ?
      /*if(ColladaExportPolicy::context() && ColladaExportPolicy::context()->mDDSInputOnly)
      {
          orkerrorlog("ERROR: <%s> Only DDS files should be referenced from DAE (and hence Maya) models! (%s)\n", InPath.c_str(),
      mFileName.c_str());

          return false;
      }
      OrkAssert(false);*/
      rv &= NvttCompress(options);
    }
  }

  return rv;
}

} // namespace ork::tool::meshutil
#endif
