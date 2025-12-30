////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Label Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct ImageView final : public Widget {
public:
  ImageView(
      const std::string& name);
  fvec4 _default_color;

  lev2::image_provider_ptr_t _imgprovider;
  lev2::texture_provider_ptr_t _texprovider;  // GPU-direct textures
  lev2::image_ptr_t _pending_image;
  lev2::image_ptr_t _active_image;
  lev2::texture_ptr_t _texture;

  lev2::uitexmaterial_ptr_t _tex_material;

  bool _maintain_aspect_ratio = false;
  bool _generate_mipmaps = false;
  bool _image_rot_180 = false;
  bool _fs_antialias = false;  // Enable adaptive Lanczos downsampling in fragment shader
  void setImage(lev2::image_ptr_t img);
  void setImageProvider(lev2::image_provider_ptr_t imgprovider);
  void setTextureProvider(lev2::texture_provider_ptr_t texprovider);
  void setTexture(lev2::texture_ptr_t tex);
  meshutil::rigidprim_V12N12B12T8C4_ptr_t _img_mesh;
  lev2::fxpipeline_ptr_t _pipeline_override;
  bool _invert_aspect = false;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;

};

} // namespace ork::ui
