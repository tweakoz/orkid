#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/object/Object.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedevcontext.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_freestyle.inl>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

class Lev2PythonApplication : public ork::Application {
  RttiDeclareConcrete(Lev2PythonApplication, ork::Application);
};

void Lev2PythonApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(Lev2PythonApplication, "Lev2PythonApplication");

void lev2appinit() {
  ork::SetCurrentThreadName("main");

  static Lev2PythonApplication the_app;
  ApplicationStack::Push(&the_app);

  int argc      = 1;
  char* argv[1] = {"python3"};

  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);

  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
}

////////////////////////////////////////////////////////

PYBIND11_MODULE(orklev2, m) {
  using namespace ork;
  using namespace ork::lev2;
  using fbi_t      = ork::python::unmanaged_ptr<FrameBufferInterface>;
  using gbi_t      = ork::python::unmanaged_ptr<GeometryBufferInterface>;
  using fxi_t      = ork::python::unmanaged_ptr<FxInterface>;
  using rsi_t      = ork::python::unmanaged_ptr<RasterStateInterface>;
  using txi_t      = ork::python::unmanaged_ptr<TextureInterface>;
  using rcfd_ptr_t = ork::python::unmanaged_ptr<const RenderContextFrameData>;
  /////////////////////////////////////////////////////////////////////////////////
  m.doc() = "Orkid Lev2 Library (graphics,audio,vr,input,etc..)";
  /////////////////////////////////////////////////////////////////////////////////
  m.def("lev2appinit", &lev2appinit);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxEnv>(m, "GfxEnv")
      .def_readonly_static("ref", &GfxEnv::GetRef())
      .def("loadingContext", &GfxEnv::loadingContext)
      .def("__repr__", [](const GfxEnv& e) -> std::string {
        fxstring<64> fxs;
        fxs.format("GfxEnv(%p)", &e);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Context>(m, "GfxContext")
      .def("mainSurfaceWidth", &Context::mainSurfaceWidth)
      .def("mainSurfaceHeight", &Context::mainSurfaceHeight)
      .def("makeCurrent", &Context::makeCurrentContext)
      .def("beginFrame", &Context::beginFrame)
      .def("endFrame", &Context::endFrame)
      .def("debugPushGroup", &Context::debugPushGroup)
      .def("debugPopGroup", &Context::debugPopGroup)
      .def("debugMarker", &Context::debugMarker)
      .def("FBI", [](Context& c) -> fbi_t { return fbi_t(c.FBI()); })
      .def("FXI", [](Context& c) -> fxi_t { return fxi_t(c.FXI()); })
      .def("GBI", [](Context& c) -> gbi_t { return gbi_t(c.GBI()); })
      .def("TXI", [](Context& c) -> txi_t { return txi_t(c.TXI()); })
      .def("RSI", [](Context& c) -> rsi_t { return rsi_t(c.RSI()); })
      .def("topRCFD", [](Context& c) -> rcfd_ptr_t { return rcfd_ptr_t(c.topRenderContextFrameData()); })
      .def_property_readonly("frameIndex", [](Context& c) -> int { return c.GetTargetFrame(); })
      .def_property("currentMaterial", &Context::currentMaterial, &Context::BindMaterial)
      .def("__repr__", [](const Context& c) -> std::string {
        fxstring<64> fxs;
        fxs.format("Context(%p)", &c);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxMaterial>(m, "GfxMaterial")
      .def_property("name", &GfxMaterial::GetName, &GfxMaterial::SetName)
      .def("__repr__", [](const GfxMaterial& m) -> std::string {
        fxstring<64> fxs;
        fxs.format("GfxMaterial(%p:%s)", &m, m.mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<FreestyleMaterial, GfxMaterial>(m, "FreestyleMaterial")
      .def(py::init<>())
      .def("gpuInit", &FreestyleMaterial::gpuInit)
      .def_readonly("shader", &FreestyleMaterial::_shader)
      .def("__repr__", [](const GfxMaterial& m) -> std::string {
        fxstring<256> fxs;
        fxs.format("FreestyleMaterial(%p:%s)", &m, m.mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<FxShader>(m, "FxShader")
      .def(py::init<>())
      .def_property("name", &FxShader::GetName, &FxShader::SetName)
      .def("__repr__", [](const FxShader& m) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", &m, m.mName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fbi_t>(m, "FrameBufferInterface")
      .def_property(
          "autoclear",
          [](const fbi_t& fbi) -> bool { return fbi.get()->GetAutoClear(); },
          [](fbi_t& fbi, bool value) { fbi.get()->SetAutoClear(value); })
      .def_property(
          "clearcolor",
          [](const fbi_t& fbi) -> fvec4 { return fbi.get()->GetClearColor(); },
          [](fbi_t& fbi, const fvec4& value) { fbi.get()->SetClearColor(value); })
      .def("capturePixel", [](const fbi_t& fbi, const fvec4& at, PixelFetchContext& pfc) { return fbi.get()->GetPixel(at, pfc); })
      .def("__repr__", [](const fbi_t& fbi) -> std::string {
        fxstring<256> fxs;
        fxs.format("FBI(%p)", fbi.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxi_t>(m, "FxInterface").def("__repr__", [](const fxi_t& fxi) -> std::string {
    fxstring<256> fxs;
    fxs.format("FXI(%p)", fxi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<gbi_t>(m, "GeometryBufferInterface").def("__repr__", [](const gbi_t& gbi) -> std::string {
    fxstring<256> fxs;
    fxs.format("GBI(%p)", gbi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<txi_t>(m, "TextureInterface").def("__repr__", [](const txi_t& txi) -> std::string {
    fxstring<256> fxs;
    fxs.format("TXI(%p)", txi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rsi_t>(m, "RasterStateInterface").def("__repr__", [](const rsi_t& rsi) -> std::string {
    fxstring<256> fxs;
    fxs.format("RSI(%p)", rsi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rcfd_ptr_t>(m, "RenderContextFrameData").def("__repr__", [](const rcfd_ptr_t& rcfd) -> std::string {
    fxstring<256> fxs;
    fxs.format("RCFD(%p)", rcfd.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PixelFetchContext>(m, "PixelFetchContext")
      .def(py::init<>())
      .def(
          "color",
          [](const PixelFetchContext& pfc, int index) -> fvec4 {
            OrkAssert(index >= 0);
            OrkAssert(index < PixelFetchContext::kmaxitems);
            return pfc.mPickColors[index];
          })
      .def("__repr__", [](const PixelFetchContext& pfc) -> std::string {
        fxstring<256> fxs;
        fxs.format("PixelFetchContext(%p)", &pfc);
        return fxs.c_str();
      });

  /////////////////////////////////////////////////////////////////////////////////
};
