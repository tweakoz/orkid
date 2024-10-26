////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

namespace dflow = dataflow;
namespace ptc = particle;

void pyinit_gfx_particles(py::module& module_lev2) {
  auto ptc_module  = module_lev2.def_submodule("particles", "lev2 dataflow particles");
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////
  auto mtl_base_type = //
      py::class_<ptc::MaterialBase, ptc::basematerial_ptr_t>(ptc_module, "MaterialBase")
      .def_property("color", 
        [](ptc::basematerial_ptr_t  m) -> fvec4 { //
          return m->_color;
        },
        [](ptc::basematerial_ptr_t  m, fvec4 color) { //
          m->_color = color;
        })
      .def_property("blending", 
        [](ptc::basematerial_ptr_t  m) -> crcstring_ptr_t { //
          auto crcstr = std::make_shared<CrcString>(uint64_t(m->_blending));
          return crcstr;
        },
        [](ptc::basematerial_ptr_t  m, crcstring_ptr_t blend) { //
          m->_blending = Blending(blend->hashed());
        })
      .def_property("depthtest", 
        [](ptc::basematerial_ptr_t  m) -> crcstring_ptr_t { //
          auto crcstr = std::make_shared<CrcString>(uint64_t(m->_depthtest));
          return crcstr;
        },
        [](ptc::basematerial_ptr_t  m, crcstring_ptr_t dtest) { //
          m->_depthtest = EDepthTest(dtest->hashed());
        });
 type_codec->registerStdCodec<ptc::basematerial_ptr_t>(mtl_base_type);
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_flat_type = //
      py::class_<ptc::FlatMaterial, ptc::MaterialBase, ptc::flatmaterial_ptr_t>(ptc_module, "FlatMaterial")
      .def_static("createShared", [] -> ptc::flatmaterial_ptr_t { return ptc::FlatMaterial::createShared(); });
  type_codec->registerStdCodec<ptc::flatmaterial_ptr_t>(mtl_flat_type);
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_grad_type = //
      py::class_<ptc::GradientMaterial, ptc::MaterialBase, ptc::gradientmaterial_ptr_t>(ptc_module, "GradientMaterial")
      .def_static("createShared", [] -> ptc::gradientmaterial_ptr_t { return ptc::GradientMaterial::createShared(); })
      .def_property("gradient", 
        [](ptc::gradientmaterial_ptr_t  m) -> gradient_fvec4_ptr_t { //
          return m->_gradient;
        },
        [](ptc::gradientmaterial_ptr_t  m, gradient_fvec4_ptr_t grad) { //
          m->_gradient = grad;
        }
        )
      .def_property("colorIntensity", 
        [](ptc::gradientmaterial_ptr_t  m) -> float { //
          return m->_gradientColorIntensity;
        },
        [](ptc::gradientmaterial_ptr_t  m, float intensity) { //
          m->_gradientColorIntensity = intensity;
        }
        )
      .def_property("alphaIntensity", 
        [](ptc::gradientmaterial_ptr_t  m) -> float { //
          return m->_gradientAlphaIntensity;
        },
        [](ptc::gradientmaterial_ptr_t  m, float intensity) { //
          m->_gradientAlphaIntensity = intensity;
        }
        )
      .def_property("modulation_texture", 
        [](ptc::gradientmaterial_ptr_t  m) -> texture_ptr_t { //
          return m->_modulation_texture;
        },
        [](ptc::gradientmaterial_ptr_t  m, texture_ptr_t t) { //
          return m->_modulation_texture = t;
        });
  type_codec->registerStdCodec<ptc::gradientmaterial_ptr_t>(mtl_grad_type);
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_tex_type = //
      py::class_<ptc::TextureMaterial, ptc::MaterialBase, ptc::texturematerial_ptr_t>(ptc_module, "TextureMaterial")
      .def_static("createShared", [] -> ptc::texturematerial_ptr_t { return ptc::TextureMaterial::createShared(); })
      .def_property("texture", 
        [](ptc::texturematerial_ptr_t  m) -> texture_ptr_t { //
          return m->_texture;
        },
        [](ptc::texturematerial_ptr_t  m, texture_ptr_t t) { //
          return m->_texture = t;
        }
        );
  type_codec->registerStdCodec<ptc::texturematerial_ptr_t>(mtl_tex_type);
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_texgrid_type = //
      py::class_<ptc::TexGridMaterial, ptc::MaterialBase, ptc::texgridmaterial_ptr_t>(ptc_module, "TexGridMaterial")
      .def_static("createShared", [] -> ptc::texgridmaterial_ptr_t { return ptc::TexGridMaterial::createShared(); })
      .def_property("texture", 
        [](ptc::texgridmaterial_ptr_t  m) -> texture_ptr_t { //
          return m->_texture;
        },
        [](ptc::texgridmaterial_ptr_t  m, texture_ptr_t t) { //
          return m->_texture = t;
        }
        )
      .def_property("gridDim", 
        [](ptc::texgridmaterial_ptr_t  m) -> float { //
          return m->_gridDim;
        },
        [](ptc::texgridmaterial_ptr_t  m, float dim) { //
          return m->_gridDim = dim;
        }
        );
  type_codec->registerStdCodec<ptc::texgridmaterial_ptr_t>(mtl_texgrid_type);
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_texvol_type = //
      py::class_<ptc::VolTexMaterial, ptc::MaterialBase, ptc::voltexmaterial_ptr_t>(ptc_module, "VolTexMaterial")
      .def_static("createShared", [] -> ptc::voltexmaterial_ptr_t { return ptc::VolTexMaterial::createShared(); });
  type_codec->registerStdCodec<ptc::voltexmaterial_ptr_t>(mtl_texvol_type);
  /////////////////////////////////////////////////////////////////////////////
  auto moduledata_type = //
      py::class_<ptc::ModuleData, dflow::DgModuleData, ptc::moduledata_ptr_t>(ptc_module, "Module")
      .def("__repr__", [](ptc::moduledata_ptr_t m) -> std::string {
          return FormatString("ptc::ModuleData(%p)", (void*)m.get());
      });
  type_codec->registerStdCodec<ptc::moduledata_ptr_t>(moduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto ptcmoduledata_type = //
      py::class_<ptc::ParticleModuleData, ptc::ModuleData, ptc::ptcmoduledata_ptr_t>(ptc_module, "ParticleModule");
  type_codec->registerStdCodec<ptc::ptcmoduledata_ptr_t>(ptcmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto globmoduledata_type = //
      py::class_<ptc::GlobalModuleData, ptc::ModuleData, ptc::globalmodule_ptr_t>(ptc_module, "Globals")
      .def_static("createShared", [] -> ptc::globalmodule_ptr_t { return ptc::GlobalModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::globalmodule_ptr_t>(globmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto poolmoduledata_type = //
      py::class_<ptc::ParticlePoolData, ptc::ModuleData, ptc::poolmodule_ptr_t>(ptc_module, "Pool")
      .def_static("createShared", [] -> ptc::poolmodule_ptr_t { return ptc::ParticlePoolData::createShared(); })
      .def_property("pool_size", 
        [](ptc::poolmodule_ptr_t  m) -> int { //
          return m->_poolSize;
        },
        [](ptc::poolmodule_ptr_t  m, int count) { //
          return m->_poolSize = count;
        }
        );
  type_codec->registerStdCodec<ptc::poolmodule_ptr_t>(poolmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto nzlmoduledata_type = //
      py::class_<ptc::NozzleEmitterData, ptc::ModuleData, ptc::nozzleemittermodule_ptr_t>(ptc_module, "NozzleEmitter")
      .def_static("createShared", [] -> ptc::nozzleemittermodule_ptr_t { return ptc::NozzleEmitterData::createShared(); });
  type_codec->registerStdCodec<ptc::nozzleemittermodule_ptr_t>(nzlmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto ringmoduledata_type = //
      py::class_<ptc::RingEmitterData, ptc::ModuleData, ptc::ringemittermodule_ptr_t>(ptc_module, "RingEmitter")
      .def_static("createShared", [] -> ptc::ringemittermodule_ptr_t { return ptc::RingEmitterData::createShared(); });
  type_codec->registerStdCodec<ptc::ringemittermodule_ptr_t>(ringmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto linemitmoduledata_type = //
      py::class_<ptc::LineEmitterData, ptc::ModuleData, ptc::lineemittermodule_ptr_t>(ptc_module, "LineEmitter")
      .def_static("createShared", [] -> ptc::lineemittermodule_ptr_t { return ptc::LineEmitterData::createShared(); });
  type_codec->registerStdCodec<ptc::lineemittermodule_ptr_t>(linemitmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto eliemitmoduledata_type = //
      py::class_<ptc::EllipticalEmitterData, ptc::ModuleData, ptc::ellipticalemittermodule_ptr_t>(ptc_module, "EllipticalEmitter")
      .def_static("createShared", [] -> ptc::ellipticalemittermodule_ptr_t { return ptc::EllipticalEmitterData::createShared(); });
  type_codec->registerStdCodec<ptc::ellipticalemittermodule_ptr_t>(eliemitmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto grvmoduledata_type = //
      py::class_<ptc::GravityModuleData, ptc::ModuleData, ptc::gravitymodule_ptr_t>(ptc_module, "Gravity")
      .def_static("createShared", [] -> ptc::gravitymodule_ptr_t { return ptc::GravityModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::gravitymodule_ptr_t>(grvmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto sphamoduledata_type = //
      py::class_<ptc::SphAttractorModuleData, ptc::ModuleData, ptc::sphattractormodule_ptr_t>(ptc_module, "SphAttractor")
      .def_static("createShared", [] -> ptc::sphattractormodule_ptr_t { return ptc::SphAttractorModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::sphattractormodule_ptr_t>(sphamoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto ellimoduledata_type = //
      py::class_<ptc::EllipticalAttractorModuleData, ptc::ModuleData, ptc::eliattractormodule_ptr_t>(ptc_module, "EllipticalAttractor")
      .def_static("createShared", [] -> ptc::eliattractormodule_ptr_t { return ptc::EllipticalAttractorModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::eliattractormodule_ptr_t>(ellimoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto pntattrmoduledata_type = //
      py::class_<ptc::PointAttractorModuleData, ptc::ModuleData, ptc::pntattractormodule_ptr_t>(ptc_module, "PointAttractor")
      .def_static("createShared", [] -> ptc::pntattractormodule_ptr_t { return ptc::PointAttractorModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::pntattractormodule_ptr_t>(pntattrmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto turbmoduledata_type = //
      py::class_<ptc::TurbulenceModuleData, ptc::ModuleData, ptc::turbulencemodule_ptr_t>(ptc_module, "Turbulence")
      .def_static("createShared", [] -> ptc::turbulencemodule_ptr_t { return ptc::TurbulenceModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::turbulencemodule_ptr_t>(turbmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto vortmoduledata_type = //
      py::class_<ptc::VortexModuleData, ptc::ModuleData, ptc::vortexmodule_ptr_t>(ptc_module, "Vortex")
      .def_static("createShared", [] -> ptc::vortexmodule_ptr_t { return ptc::VortexModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::vortexmodule_ptr_t>(vortmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto dragmoduledata_type = //
      py::class_<ptc::DragModuleData, ptc::ModuleData, ptc::dragmodule_ptr_t>(ptc_module, "Drag")
      .def_static("createShared", [] -> ptc::dragmodule_ptr_t { return ptc::DragModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::dragmodule_ptr_t>(dragmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto lightmoduledata_type = //
      py::class_<ptc::LightRendererData, ptc::ModuleData, ptc::lightmodule_ptr_t>(ptc_module, "LightRenderer")
      .def_static("createShared", [] -> ptc::lightmodule_ptr_t { return ptc::LightRendererData::createShared(); });
  type_codec->registerStdCodec<ptc::lightmodule_ptr_t>(lightmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto spritemoduledata_type = //
      py::class_<ptc::SpriteRendererData, ptc::ModuleData, ptc::spritemodule_ptr_t>(ptc_module, "SpriteRenderer")
      .def_property("material",[](ptc::spritemodule_ptr_t r)->ptc::basematerial_ptr_t{
        return r->_material;
      },
      [](ptc::spritemodule_ptr_t r, ptc::basematerial_ptr_t m){
        r->_material = m;
      })      
      .def_property("depth_sort", 
        [](ptc::spritemodule_ptr_t  m) -> bool { //
          return m->_sort;
        },
        [](ptc::spritemodule_ptr_t  m, bool sort) { //
          return m->_sort = sort;
        }
        )
      .def_static("createShared", [] -> ptc::spritemodule_ptr_t { return ptc::SpriteRendererData::createShared(); });
  /////////////////////////////////////////////////////////////////////////////
  auto streakmoduledata_type = //
      py::class_<ptc::StreakRendererData, ptc::ModuleData, ptc::streakmodule_ptr_t>(ptc_module, "StreakRenderer")
      .def_property("material",[](ptc::streakmodule_ptr_t r)->ptc::basematerial_ptr_t{
        return r->_material;
      },
      [](ptc::streakmodule_ptr_t r, ptc::basematerial_ptr_t m){
        r->_material = m;
      })   
      .def_property("depth_sort", 
        [](ptc::streakmodule_ptr_t  m) -> bool { //
          return m->_sort;
        },
        [](ptc::streakmodule_ptr_t  m, bool sort) { //
          return m->_sort = sort;
        }
        )
      .def_static("createShared", [] -> ptc::streakmodule_ptr_t { return ptc::StreakRendererData::createShared(); });
  type_codec->registerStdCodec<ptc::streakmodule_ptr_t>(streakmoduledata_type);
}

} //namespace ork::lev2 {
