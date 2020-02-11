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
  using fxshader_t = ork::python::unmanaged_ptr<FxShader>;
  using fxparam_t  = ork::python::unmanaged_ptr<FxShaderParam>;
  using fxtechnique_t  = ork::python::unmanaged_ptr<FxShaderTechnique>;
  using rcfd_ptr_t = ork::python::unmanaged_ptr<RenderContextFrameData>;
  using fxparammap_t = std::map<std::string,fxparam_t>;
  using fxtechniquemap_t = std::map<std::string,fxtechnique_t>;
  using vtxa_t = SVtxV12N12B12T8C4;
  using vb_static_vtxa_t = StaticVertexBuffer<vtxa_t>;
  using vw_vtxa_t = VtxWriter<vtxa_t>;
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
      .def("topRCFD", [](Context& c) -> rcfd_ptr_t {
        auto rcfd = c.topRenderContextFrameData();
        return rcfd_ptr_t(const_cast<RenderContextFrameData*>(rcfd));
      })
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
      .def_property_readonly("shader", [](const FreestyleMaterial& m) -> fxshader_t {
        return fxshader_t(m._shader);
      })
      .def("bindTechnique",  [](FreestyleMaterial& m,const fxtechnique_t& tek) {
        m.bindTechnique(tek.get());
      })
      .def("bindParamFloat",  [](FreestyleMaterial& m,fxparam_t& p,float value) {
        m.bindParamFloat(p.get(),value);
      })
      .def("bindParamVec2",  [](FreestyleMaterial& m,fxparam_t& p,const fvec2& value) {
        m.bindParamVec2(p.get(),value);
      })
      .def("bindParamVec3",  [](FreestyleMaterial& m,fxparam_t& p,const fvec3& value) {
        m.bindParamVec3(p.get(),value);
      })
      .def("bindParamVec4",  [](FreestyleMaterial& m,fxparam_t& p,const fvec4& value) {
        m.bindParamVec4(p.get(),value);
      })
      .def("bindParamMatrix",  [](FreestyleMaterial& m,fxparam_t& p,const fmtx4& value) {
        m.bindParamMatrix(p.get(),value);
      })
      .def("begin",  [](FreestyleMaterial& m,rcfd_ptr_t& rcfd) {
        m.begin(*rcfd.get());
      })
      .def("end",  [](FreestyleMaterial& m,rcfd_ptr_t& rcfd) {
        m.end(*rcfd.get());
      })
      .def("__repr__", [](const FreestyleMaterial& m) -> std::string {
        fxstring<256> fxs;
        fxs.format("FreestyleMaterial(%p:%s)", &m, m.mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxshader_t>(m, "FxShader")
      .def(py::init<>())
      .def_property_readonly("name", [](const fxshader_t& sh) -> std::string {
        return sh->mName;
      })
      .def_property_readonly("params",  [](const fxshader_t& sh) -> fxparammap_t {
        fxparammap_t rval;
        for( auto item : sh->_parameterByName ){
          // python has no concept of const
          //  so we must cast away constness
          rval[item.first] = fxparam_t(const_cast<FxShaderParam*>(item.second));
        }
        return rval;
      })
      .def("param",  [](const fxshader_t& sh,const std::string& named) -> fxparam_t {
        auto it = sh->_parameterByName.find(named);
        fxparam_t rval(nullptr);
        if(it!=sh->_parameterByName.end())
          rval = fxparam_t(const_cast<FxShaderParam*>(it->second));
        return rval;
      })
      .def_property_readonly("techniques",  [](const fxshader_t& sh) -> fxtechniquemap_t {
        fxtechniquemap_t rval;
        for( auto item : sh->_techniques ){
          // python has no concept of const
          //  so we must cast away constness
          rval[item.first] = fxtechnique_t(const_cast<FxShaderTechnique*>(item.second));
        }
        return rval;
      })
      .def("technique",  [](const fxshader_t& sh,const std::string& named) -> fxtechnique_t {
        auto it = sh->_techniques.find(named);
        fxtechnique_t rval(nullptr);
        if(it!=sh->_techniques.end())
          rval = fxtechnique_t(const_cast<FxShaderTechnique*>(it->second));
        return rval;
      })
      .def("__repr__", [](const fxshader_t& sh) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", sh.get(), sh->mName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxparam_t>(m, "FxShaderParam")
      .def_property_readonly("name",  [](const fxparam_t& p) -> std::string {
        return p->_name;
      })
      .def("__repr__", [](const fxparam_t& p) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", p.get(), p->_name.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxtechnique_t>(m, "FxShaderTechnique")
      .def_property_readonly("name",  [](const fxtechnique_t& t) -> std::string {
        return t->mTechniqueName;
      })
      .def("__repr__", [](const fxtechnique_t& t) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShaderTechnique(%p:%s)", t.get(), t->mTechniqueName.c_str());
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
  })
  .def("lock",[](gbi_t gbi,vb_static_vtxa_t& vb,int icount)->vw_vtxa_t{
    vw_vtxa_t vw;
    vw.Lock(gbi.get(),&vb,icount);
    return vw;
  })
  .def("unlock",[](gbi_t gbi,vw_vtxa_t& vw){
    vw.UnLock(gbi.get());
  })
  .def("drawTriangles",[](gbi_t gbi,vw_vtxa_t& vw){
    gbi.get()->DrawPrimitiveEML(vw,EPRIM_TRIANGLES);
  })
  .def("drawTriangleStrip",[](gbi_t gbi,vw_vtxa_t& vw){
    gbi.get()->DrawPrimitiveEML(vw,EPRIM_TRIANGLESTRIP);
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vw_vtxa_t>(m, "Writer_V12N12B12T8C4").def("__repr__", [](const vw_vtxa_t& vw) -> std::string {
    fxstring<256> fxs;
    fxs.format("Writer_V12N12B12T8C4(%p)", &vw);
    return fxs.c_str();
  })
  .def("add",[](vw_vtxa_t&vw, vtxa_t& vtx){
    vw.AddVertex(vtx);
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
  py::class_<VertexBufferBase>(m, "VertexBufferBase");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vtxa_t>(m, "VtxV12N12B12T8C4")
    .def(py::init<fvec3,fvec3,fvec3,fvec2,uint32_t>())
    .def_static("staticBuffer",[](size_t size)->vb_static_vtxa_t{
      return vb_static_vtxa_t(size,0,EPRIM_NONE);
    });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vb_static_vtxa_t,VertexBufferBase>(m, "VtxV12N12B12T8C4_StaticBuffer");
  /////////////////////////////////////////////////////////////////////////////////
};
