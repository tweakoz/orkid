////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

  void MaterialBase::describeX(class_t* clazz) {
  }
  ///////////////////////////////////////////////////////////////////////////////
  void TextureMaterial::describeX(class_t* clazz) {
    //ork::reflect::RegisterProperty("Texture", &TextureMaterial::GetTextureAccessor, &TextureMaterial::SetTextureAccessor);
    //ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.class", "ged.factory.assetlist");
    //ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.assettype", "lev2tex");
    //ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.assetclass", "lev2tex");
  }
  ///////////////////////////////////////////////////////////////////////////////
  TextureMaterial::TextureMaterial()
      : _texture(nullptr) {
    auto targ = lev2::contextForCurrentThread();
    _material               = std::make_shared<GfxMaterial3DSolid>(targ, "orkshader://particle", "tbasicparticle");
    _material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
  }
  ///////////////////////////////////////////////////////////////////////////////
  void TextureMaterial::update(float ftexframe) {
    auto targ = lev2::contextForCurrentThread();
    if (targ && _texture) {
      lev2::TextureAnimationBase* texanim = _texture->GetTexAnim();

      if (texanim) {
        TextureAnimationInst tai(texanim);
        tai.SetCurrentTime(ftexframe);
        targ->TXI()->UpdateAnimatedTexture(_texture.get(), &tai);
      }
    }
  }
  ///////////////////////////////////////////////////////////////////////////////
  test_mtl_ptr_t TextureMaterial::bind(Context* pT) {

    _material->SetTexture(_texture.get());
    _material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
    _material->_rasterstate.SetAlphaTest(EALPHATEST_GREATER, 0.0f);
    _material->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
    _material->_rasterstate.SetZWriteMask(false);
    _material->_rasterstate.SetCullTest(ECullTest::OFF);
    _material->_rasterstate.SetPointSize(32.0f);
    return _material;
  }

/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::TextureMaterial, "psys::TextureMaterial");
ImplementReflectionX(ptcl::MaterialBase, "psys::MaterialBase");
