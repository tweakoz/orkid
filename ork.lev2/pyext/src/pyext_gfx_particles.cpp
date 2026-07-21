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
#include <ork/lev2/gfx/particle/vdbcollider_holder.inl>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/asset_gen.h> // material_gen (PbrMaterialGenData)

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
          m->_blending = BlendingMacro(blend->hashed());
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
      .def_property("modtexture_asset",
        [](ptc::gradientmaterial_ptr_t m) -> std::string {
          return m->_modulation_texture_asset ? std::string(m->_modulation_texture_asset->_name.c_str()) : std::string();
        },
        [](ptc::gradientmaterial_ptr_t m, std::string path) {
          auto req = std::make_shared<asset::LoadRequest>(path.c_str());
          auto ass = asset::AssetManager<TextureAsset>::load(req);
          m->_modulation_texture_asset = ass;
          if (ass)
            m->_modulation_texture = ass->GetTexture();
        })
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
  // GradientAtlasMaterial — gradient-via-2D-texture-atlas. atlas is the
  // user-provided texture (X = unit_age, Y = aux.x). Same intensity /
  // modulation_texture controls as GradientMaterial for familiarity.
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_atlas_type = //
      py::class_<ptc::GradientAtlasMaterial, ptc::MaterialBase, ptc::gradientatlasmaterial_ptr_t>(ptc_module, "GradientAtlasMaterial")
      .def_static("createShared", [] -> ptc::gradientatlasmaterial_ptr_t { return ptc::GradientAtlasMaterial::createShared(); })
      .def_property("atlas",
        [](ptc::gradientatlasmaterial_ptr_t m) -> texture_ptr_t { return m->_atlas; },
        [](ptc::gradientatlasmaterial_ptr_t m, texture_ptr_t t) { m->_atlas = t; })
      .def_property("colorIntensity",
        [](ptc::gradientatlasmaterial_ptr_t m) -> float { return m->_gradientColorIntensity; },
        [](ptc::gradientatlasmaterial_ptr_t m, float v) { m->_gradientColorIntensity = v; })
      .def_property("alphaIntensity",
        [](ptc::gradientatlasmaterial_ptr_t m) -> float { return m->_gradientAlphaIntensity; },
        [](ptc::gradientatlasmaterial_ptr_t m, float v) { m->_gradientAlphaIntensity = v; })
      .def_property("modulation_texture",
        [](ptc::gradientatlasmaterial_ptr_t m) -> texture_ptr_t { return m->_modulation_texture; },
        [](ptc::gradientatlasmaterial_ptr_t m, texture_ptr_t t) { m->_modulation_texture = t; });
  type_codec->registerStdCodec<ptc::gradientatlasmaterial_ptr_t>(mtl_atlas_type);
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
      .def_property("texture_asset",
        [](ptc::texgridmaterial_ptr_t m) -> std::string {
          return m->_texture_asset ? std::string(m->_texture_asset->_name.c_str()) : std::string();
        },
        [](ptc::texgridmaterial_ptr_t m, std::string path) {
          // the SERIALIZABLE texture form (directAssetProperty) — also sets
          // the live texture so in-process (viewer) renders identically.
          auto req         = std::make_shared<asset::LoadRequest>(path.c_str());
          auto ass         = asset::AssetManager<TextureAsset>::load(req);
          m->_texture_asset = ass;
          if (ass)
            m->_texture = ass->GetTexture();
        })
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
  // FreestyleParticleMaterial — flipbook cookie x gradient ramp, PREMA-first.
  // ONE chain renders additive fire morphing into absorptive smoke; shader_path
  // swaps in a custom fxv2 (DSL / aux-channel hook). color/blending/depthtest
  // come from the MaterialBase bindings.
  /////////////////////////////////////////////////////////////////////////////
  auto mtl_freestyle_type = //
      py::class_<ptc::FreestyleParticleMaterial, ptc::MaterialBase, ptc::freestyleparticlematerial_ptr_t>(ptc_module, "FreestyleParticleMaterial")
      .def_static("createShared", [] -> ptc::freestyleparticlematerial_ptr_t { return ptc::FreestyleParticleMaterial::createShared(); })
      .def_property("texture_asset",
        [](ptc::freestyleparticlematerial_ptr_t m) -> std::string {
          return m->_texture_asset ? std::string(m->_texture_asset->_name.c_str()) : std::string();
        },
        [](ptc::freestyleparticlematerial_ptr_t m, std::string path) {
          // the SERIALIZABLE texture form (directAssetProperty) — also sets
          // the live texture so in-process (viewer) renders identically.
          auto req          = std::make_shared<asset::LoadRequest>(path.c_str());
          auto ass          = asset::AssetManager<TextureAsset>::load(req);
          m->_texture_asset = ass;
          if (ass)
            m->_texture = ass->GetTexture();
        })
      .def_property("gradient",
        [](ptc::freestyleparticlematerial_ptr_t m) -> gradient_fvec4_ptr_t { return m->_gradient; },
        [](ptc::freestyleparticlematerial_ptr_t m, gradient_fvec4_ptr_t grad) { m->_gradient = grad; })
      .def_property("gridDim",
        [](ptc::freestyleparticlematerial_ptr_t m) -> float { return m->_gridDim; },
        [](ptc::freestyleparticlematerial_ptr_t m, float dim) { m->_gridDim = dim; })
      .def_property("colorIntensity",
        [](ptc::freestyleparticlematerial_ptr_t m) -> float { return m->_gradientColorIntensity; },
        [](ptc::freestyleparticlematerial_ptr_t m, float v) { m->_gradientColorIntensity = v; })
      .def_property("alphaIntensity",
        [](ptc::freestyleparticlematerial_ptr_t m) -> float { return m->_gradientAlphaIntensity; },
        [](ptc::freestyleparticlematerial_ptr_t m, float v) { m->_gradientAlphaIntensity = v; })
      .def_property("shader_path",
        [](ptc::freestyleparticlematerial_ptr_t m) -> std::string { return m->_shader_path; },
        [](ptc::freestyleparticlematerial_ptr_t m, std::string path) { m->_shader_path = path; })
      .def_property("emission_smoothing",
        [](ptc::freestyleparticlematerial_ptr_t m) -> float { return m->_emission_smoothing; },
        [](ptc::freestyleparticlematerial_ptr_t m, float v) { m->_emission_smoothing = v; })
      .def_property("emission_lum_power",
        [](ptc::freestyleparticlematerial_ptr_t m) -> float { return m->_emission_lum_power; },
        [](ptc::freestyleparticlematerial_ptr_t m, float v) { m->_emission_lum_power = v; })
      .def_property("emission_tint",
        [](ptc::freestyleparticlematerial_ptr_t m) -> fvec3 { return m->_emission_tint; },
        [](ptc::freestyleparticlematerial_ptr_t m, fvec3 v) { m->_emission_tint = v; });
  type_codec->registerStdCodec<ptc::freestyleparticlematerial_ptr_t>(mtl_freestyle_type);
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
  // EntityRef — parametric SRT decomposition of a published entity
  // transform. Created lazily by the DSL lowerer for each unique
  // Expr.entity("name") reference.
  auto entrefdata_type = //
      py::class_<ptc::EntityRefModuleData, ptc::ModuleData, ptc::entityrefmodule_ptr_t>(ptc_module, "EntityRef")
      .def_static("createShared",
          [] -> ptc::entityrefmodule_ptr_t {
            return ptc::EntityRefModuleData::createShared();
          })
      .def_static("createWithName",
          [](std::string name) -> ptc::entityrefmodule_ptr_t {
            return ptc::EntityRefModuleData::createWithName(name);
          })
      .def_property(
          "entity_name",
          [](ptc::entityrefmodule_ptr_t e) -> std::string { return e->_entity_name; },
          [](ptc::entityrefmodule_ptr_t e, std::string v) { e->_entity_name = v; });
  type_codec->registerStdCodec<ptc::entityrefmodule_ptr_t>(entrefdata_type);
  /////////////////////////////////////////////////////////////////////////////
  // TransformPoint — local_point -> host_xf * local_point.
  auto txp_type = //
      py::class_<ptc::TransformPointModuleData, ptc::ModuleData, ptc::transform_point_module_ptr_t>(ptc_module, "TransformPoint")
      .def_static("createShared",
          [] -> ptc::transform_point_module_ptr_t {
            return ptc::TransformPointModuleData::createShared();
          })
      .def_property(
          "entity_name",
          [](ptc::transform_point_module_ptr_t m) -> std::string { return m->_entity_name; },
          [](ptc::transform_point_module_ptr_t m, std::string v) { m->_entity_name = v; });
  type_codec->registerStdCodec<ptc::transform_point_module_ptr_t>(txp_type);
  /////////////////////////////////////////////////////////////////////////////
  // TransformDir — 3x3 of host_xf applied to a local direction.
  auto txd_type = //
      py::class_<ptc::TransformDirModuleData, ptc::ModuleData, ptc::transform_dir_module_ptr_t>(ptc_module, "TransformDir")
      .def_static("createShared",
          [] -> ptc::transform_dir_module_ptr_t {
            return ptc::TransformDirModuleData::createShared();
          })
      .def_property(
          "entity_name",
          [](ptc::transform_dir_module_ptr_t m) -> std::string { return m->_entity_name; },
          [](ptc::transform_dir_module_ptr_t m, std::string v) { m->_entity_name = v; });
  type_codec->registerStdCodec<ptc::transform_dir_module_ptr_t>(txd_type);
  /////////////////////////////////////////////////////////////////////////////
  // Vec3Add — A + B = Sum, componentwise.
  auto v3add_type = //
      py::class_<ptc::Vec3AddModuleData, ptc::ModuleData, ptc::vec3add_module_ptr_t>(ptc_module, "Vec3Add")
      .def_static("createShared",
          [] -> ptc::vec3add_module_ptr_t {
            return ptc::Vec3AddModuleData::createShared();
          });
  type_codec->registerStdCodec<ptc::vec3add_module_ptr_t>(v3add_type);
  /////////////////////////////////////////////////////////////////////////////
  // Vec3Combine — three scalar inputs X/Y/Z + one fvec3 output "value".
  // Emitted by the HyperSyn DSL lowerer for Expr.vec3(...) bindings.
  auto vec3combinedata_type = //
      py::class_<ptc::Vec3CombineModuleData, ptc::ModuleData, ptc::vec3combinemodule_ptr_t>(ptc_module, "Vec3Combine")
      .def_static("createShared", [] -> ptc::vec3combinemodule_ptr_t { return ptc::Vec3CombineModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::vec3combinemodule_ptr_t>(vec3combinedata_type);
  /////////////////////////////////////////////////////////////////////////////
  // Parameters — runtime-mutable scalar source. DSL self.expose(name, default)
  // adds a named float output plug; gameplay SET_PARAM mutates the value.
  auto paramsdata_type = //
      py::class_<ptc::ParametersModuleData, ptc::ModuleData, ptc::parametersmodule_ptr_t>(ptc_module, "Parameters")
      .def_static("createShared", [] -> ptc::parametersmodule_ptr_t { return ptc::ParametersModuleData::createShared(); })
      .def("addFloatParam",
          [](ptc::parametersmodule_ptr_t p, std::string name, float def) {
            p->addFloatParam(name, def);
          })
      .def_property_readonly("param_names",
          [](ptc::parametersmodule_ptr_t p) -> std::vector<std::string> {
            return p->paramNames();
          });
  type_codec->registerStdCodec<ptc::parametersmodule_ptr_t>(paramsdata_type);
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
  auto dirforcemoduledata_type = //
      py::class_<ptc::DirectionalForceModuleData, ptc::ModuleData, ptc::directional_force_module_ptr_t>(ptc_module, "DirectionalForce")
      .def_static("createShared",
          [] -> ptc::directional_force_module_ptr_t {
            return ptc::DirectionalForceModuleData::createShared();
          });
  type_codec->registerStdCodec<ptc::directional_force_module_ptr_t>(dirforcemoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto exprforcemoduledata_type = // E2.5 S8: ExprIR-driven per-particle force (particles.force ctx)
      py::class_<ptc::ExprForceModuleData, ptc::ModuleData, ptc::expr_force_module_ptr_t>(ptc_module, "ExprForce")
      .def_static("createShared", [] -> ptc::expr_force_module_ptr_t { return ptc::ExprForceModuleData::createShared(); })
      .def_property("force_x", [](ptc::expr_force_module_ptr_t e) { return e->_force_x; },
                               [](ptc::expr_force_module_ptr_t e, std::string s) { e->_force_x = s; })
      .def_property("force_y", [](ptc::expr_force_module_ptr_t e) { return e->_force_y; },
                               [](ptc::expr_force_module_ptr_t e, std::string s) { e->_force_y = s; })
      .def_property("force_z", [](ptc::expr_force_module_ptr_t e) { return e->_force_z; },
                               [](ptc::expr_force_module_ptr_t e, std::string s) { e->_force_z = s; });
  type_codec->registerStdCodec<ptc::expr_force_module_ptr_t>(exprforcemoduledata_type);
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
  /////////////////////////////////////////////////////////////////////////////
  py::class_<ptc::CurlNoiseForceModuleData, ptc::ModuleData, ptc::curlnoiseforce_ptr_t>(ptc_module, "CurlNoiseForce")
      .def_static("createShared", [] -> ptc::curlnoiseforce_ptr_t { return ptc::CurlNoiseForceModuleData::createShared(); });
  /////////////////////////////////////////////////////////////////////////////
  py::class_<ptc::PolyDragModuleData, ptc::ModuleData, ptc::polydrag_ptr_t>(ptc_module, "PolyDrag")
      .def_static("createShared", [] -> ptc::polydrag_ptr_t { return ptc::PolyDragModuleData::createShared(); });
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
  auto planecollidermoduledata_type = //
      py::class_<ptc::PlaneColliderModuleData, ptc::ModuleData, ptc::planecollider_ptr_t>(ptc_module, "PlaneCollider")
      .def_static("createShared", [] -> ptc::planecollider_ptr_t { return ptc::PlaneColliderModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::planecollider_ptr_t>(planecollidermoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto spherecollidermoduledata_type = //
      py::class_<ptc::SphereColliderModuleData, ptc::ModuleData, ptc::spherecollider_ptr_t>(ptc_module, "SphereCollider")
      .def_static("createShared", [] -> ptc::spherecollider_ptr_t { return ptc::SphereColliderModuleData::createShared(); });
  type_codec->registerStdCodec<ptc::spherecollider_ptr_t>(spherecollidermoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto vdbcollidermoduledata_type = //
      py::class_<ptc::VdbColliderModuleData, ptc::ModuleData, ptc::vdbcollider_ptr_t>(ptc_module, "VdbCollider")
      .def_static("createShared", [] -> ptc::vdbcollider_ptr_t { return ptc::VdbColliderModuleData::createShared(); })
      // .sdf_grid = <FloatGrid> — stores the grid as the collider's SDF
      // source. Wraps in an opaque holder so the header (modular_forces.h)
      // stays free of <openvdb> includes. None clears the grid.
      .def_property(
          "sdf_grid",
          [](ptc::vdbcollider_ptr_t m) -> lev2::vdb_floatgrid_ptr_t {
            if (m->_sdfGrid) return m->_sdfGrid->grid;
            return nullptr;
          },
          [](ptc::vdbcollider_ptr_t m, lev2::vdb_floatgrid_ptr_t g) {
            if (g) {
              auto h = std::make_shared<ptc::VdbColliderGridHolder>();
              h->grid = g;
              m->_sdfGrid = h;
            } else {
              m->_sdfGrid.reset();
            }
          })
      // .follow_entity = "<publish_name>" — when set, the collider
      // tracks the named entity's world transform every tick. Looked up
      // via GraphInst::_resolveEntityXf at compute time. Empty (default)
      // → world-space sampling (legacy behavior). See
      // SpawnData::_publishxf_name on the publisher side.
      .def_property(
          "follow_entity",
          [](ptc::vdbcollider_ptr_t m) -> std::string {
            return m->_follow_entity;
          },
          [](ptc::vdbcollider_ptr_t m, const std::string& s) {
            m->_follow_entity = s;
          })
      // .sdf_offset = vec3 — local-frame placement offset of the COLLISION surface
      // (meters); lets collision sit slightly off the shared visual surface.
      .def_property(
          "sdf_offset",
          [](ptc::vdbcollider_ptr_t m) -> fvec3 { return m->_sdf_offset; },
          [](ptc::vdbcollider_ptr_t m, fvec3 v) { m->_sdf_offset = v; })
      // .sdf_asset = "<asset_name>" — the REFLECTED asset reference; the host
      // resolves it to the live grid post-deserialize (resolve_sdf_assets below).
      .def_property(
          "sdf_asset",
          [](ptc::vdbcollider_ptr_t m) -> std::string {
            return m->_sdf_asset_name;
          },
          [](ptc::vdbcollider_ptr_t m, const std::string& s) {
            m->_sdf_asset_name = s;
          });
  type_codec->registerStdCodec<ptc::vdbcollider_ptr_t>(vdbcollidermoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  // resolve_sdf_assets(graphdata, asset_name, grid) — fill the live grid on every
  // module of `graphdata` whose reflected sdf_asset matches `asset_name` (the
  // post-deserialize wire step for model-B graphs whose collider references its SDF
  // by name). Returns the number of modules resolved.
  ptc_module.def(
      "resolve_sdf_assets",
      [](dflow::graphdata_ptr_t g, const std::string& asset_name, lev2::vdb_floatgrid_ptr_t grid) -> int {
        int count = 0;
        for (size_t i = 0; i < g->numModules(); i++) {
          auto vdbc = std::dynamic_pointer_cast<ptc::VdbColliderModuleData>(g->module(i));
          if (vdbc and vdbc->_sdf_asset_name == asset_name) {
            auto h  = std::make_shared<ptc::VdbColliderGridHolder>();
            h->grid = grid;
            vdbc->_sdfGrid = h;
            count++;
          }
        }
        return count;
      });
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
      .def_property("draw_order",
        [](ptc::spritemodule_ptr_t  m) -> int { return m->_draw_order; },
        [](ptc::spritemodule_ptr_t  m, int order) { m->_draw_order = order; })
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
      .def_property("draw_order",
        [](ptc::streakmodule_ptr_t  m) -> int { return m->_draw_order; },
        [](ptc::streakmodule_ptr_t  m, int order) { m->_draw_order = order; })
      .def_static("createShared", [] -> ptc::streakmodule_ptr_t { return ptc::StreakRendererData::createShared(); });
  type_codec->registerStdCodec<ptc::streakmodule_ptr_t>(streakmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  // VdbLevelSetRenderer — splat particles to VDB level set, marching cubes,
  // draw resulting triangle mesh through the standard material pipeline.
  /////////////////////////////////////////////////////////////////////////////
  // Kernel selection follows the BlendingMacro / EDepthTest pattern:
  // CrcString-keyed. Author writes `renderer.kernel = tokens.Wyvill`
  // (or .Cubic / .Quartic / .Gaussian). The enum's underlying values
  // are the CRCs of those names so the cast is direct.
  auto vdbls_type = //
      py::class_<ptc::VdbLevelSetRendererData, ptc::ModuleData, ptc::vdblevelset_module_ptr_t>(ptc_module, "VdbLevelSetRenderer")
      .def_static("createShared", []() -> ptc::vdblevelset_module_ptr_t {
        return ptc::VdbLevelSetRendererData::createShared();
      })
      .def_property("material",
        [](ptc::vdblevelset_module_ptr_t m) -> material_ptr_t { return m->_material; },
        [](ptc::vdblevelset_module_ptr_t m, material_ptr_t mat) { m->_material = mat; })
      // .material_gen = <PbrMaterialGenData> — the SERIALIZABLE material recipe;
      // re-materialized at first render when the live .material is absent (round-trip).
      .def_property("material_gen",
        [](ptc::vdblevelset_module_ptr_t m) -> pbr_material_gendata_ptr_t { return m->_material_gen; },
        [](ptc::vdblevelset_module_ptr_t m, pbr_material_gendata_ptr_t g) { m->_material_gen = g; })
      .def_property("voxel_size",
        [](ptc::vdblevelset_module_ptr_t m) -> float { return m->_voxelSize; },
        [](ptc::vdblevelset_module_ptr_t m, float v) { m->_voxelSize = v; })
      .def_property("kernel",
        [](ptc::vdblevelset_module_ptr_t m) -> crcstring_ptr_t {
          return std::make_shared<CrcString>(uint64_t(m->_kernel));
        },
        [](ptc::vdblevelset_module_ptr_t m, crcstring_ptr_t cs) {
          m->_kernel = ptc::VdbLevelSetKernel(cs->hashed());
        });
  type_codec->registerStdCodec<ptc::vdblevelset_module_ptr_t>(vdbls_type);
}

} //namespace ork::lev2 {
