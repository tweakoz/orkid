#include <Python.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/fixedstring.hpp>
#include <pkg/ent/scene.h>
#include <ostream>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

namespace std
{
    ostream& operator<<(ostream& ostr,const ::ork::fvec3& vec3)
    {
        ostr<<"("<<vec3.GetX()<<","<<vec3.GetY()<<","<<vec3.GetZ()<<")";
        return ostr;
    }
    ostream& operator<<(ostream& ostr,const ::ork::fvec2& vec2)
    {
        ostr<<"("<<vec2.GetX()<<","<<vec2.GetY()<<")";
        return ostr;
    }
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////
ent::SceneData* PyNewScene();
ent::SceneData* PyGetScene();
void PyNewRefArch(const std::string& name);
void PyNewArch(const std::string& classname,const std::string& name);
void PyNewEntity(const std::string& name,const std::string& archname="");
///////////////////////////////////////////////////////////////////////////////

class ed
{
public:
    std::string whatup() { return std::string( "whatup yourself" ); }
    std::string damn() { return std::string( "hot damn" ); }
    ent::SceneData* getscene() { return PyGetScene(); }
    ent::SceneData* newscene() { return PyNewScene(); }
    void newentity(const std::string&entname,const std::string&archname="no_arch")
    {
        PyNewEntity(entname,archname);
    }
    void newrefarch(const std::string&name)
    {
        PyNewRefArch(name);
    }
    void newarch(const std::string&classname,const std::string&name)
    {
        PyNewArch(classname,name);
    }
};

///////////////////////////////////////////////////////////////////////////////

void orkpy_initork()
{
    ////////////////////////
    // ork::fvec3
    ////////////////////////

    py::module mm("__main__","Orkid");
    py::object main_namespace = mm.attr("__dict__");

    main_namespace["vec2"] = py::class_<fvec2>(mm,"vec2")
                            .def(py::init<>())
                            .def(py::init<float,float>())
                            .def_property("x", &fvec2::GetX, &fvec2::SetX)
                            .def_property("y", &fvec2::GetY, &fvec2::SetY)
                            .def("dot",&fvec2::Dot) // __add__
                            .def("perp",&fvec2::PerpDot) // __add__
                            .def("mag",&fvec2::Mag) // __add__
                            .def("magsquared",&fvec2::MagSquared) // __add__
                            .def("lerp",&fvec2::Lerp) // __add__
                            .def("serp",&fvec2::Serp) // __add__
                            .def("normal",&fvec2::Normal) // __add__
                            .def("normalize",&fvec2::Normalize) // __add__
                            .def(py::self + py::self) // __add__
                            .def(py::self - py::self) // __sub__
                            .def(py::self * py::self) // __scalar mul__
                            .def("__str__", [](const fvec2& v)->std::string
                            {
                                fxstring<64> fxs; fxs.format("vec2(%g,%g)",v.x,v.y);
                                return fxs.c_str();
                            });
                            //.def(self_ns::str(self));

    main_namespace["vec3"] = py::class_<fvec3>(mm,"vec3")
                            .def(py::init<>())
                            .def(py::init<float,float,float>())
                            .def_property("x", &fvec3::GetX, &fvec3::SetX)
                            .def_property("y", &fvec3::GetY, &fvec3::SetY)
                            .def_property("z", &fvec3::GetZ, &fvec3::SetZ)
                            .def("dot",&fvec3::Dot) // __add__
                            .def("cross",&fvec3::Cross) // __add__
                            .def("mag",&fvec3::Mag) // __add__
                            .def("magsquared",&fvec3::MagSquared) // __add__
                            .def("lerp",&fvec3::Lerp) // __add__
                            .def("serp",&fvec3::Serp) // __add__
                            .def("reflect",&fvec3::Reflect) // __add__
                            .def("saturate",&fvec3::Saturate) // __add__
                            .def("normal",&fvec3::Normal) // __add__
                            .def("normalize",&fvec3::Normalize) // __add__
                            .def("rotx",&fvec3::RotateX) // __add__
                            .def("roty",&fvec3::RotateY) // __add__
                            .def("rotz",&fvec3::RotateZ) // __add__
                            .def(py::self + py::self) // __add__
                            .def(py::self - py::self) // __sub__
                            .def(py::self * py::self)// // __scalar mul__
                            .def("__str__", [](const fvec3& v)->std::string
                            {
                                fxstring<64> fxs; fxs.format("vec3(%g,%g,%g)",v.x,v.y,v.z);
                                return fxs.c_str();
                            });

    main_namespace["object"] =
        py::class_<ork::Object>(mm,"ork::Object")
        .def("clazz",[](ork::Object*o)->std::string{
            auto clazz = rtti::downcast<object::ObjectClass*>( o->GetClass() );
            auto name = clazz->Name();
            return name.c_str();
        });

    main_namespace["scene"] =
        py::class_<ent::SceneData>(mm,"Scene")
        .def("objects",[](ent::SceneData*sd)->std::list<std::pair<std::string,ork::Object*>>{

            std::list<std::pair<std::string,ork::Object*>> rval;
            auto& objs = sd->GetSceneObjects();
            for(const auto& item : objs )
            {
                auto name = item.first.c_str();
                auto obj = (ork::Object*) item.second;
                auto p = std::pair<std::string,ork::Object*>(name,obj);
                rval.push_back(p);
            }

            return rval;

        });

    ////////////////////////
    // scene editor
    ////////////////////////

    main_namespace["editor"] = py::class_<ed>(mm,"editor")
                            .def(py::init<>())
                            .def("whatup",&ed::whatup)
                            .def("damn",&ed::damn)
                            .def("newscene",&ed::newscene)
                            .def("newentity",&ed::newentity)
                            .def("newrefarch",&ed::newrefarch)
                            .def("newarch",&ed::newarch)
                            .def("s",&ed::getscene)
                            .def("ns",&ed::newscene)
                            .def("ne",&ed::newentity)
                            .def("na",&ed::newarch)
                            .def("nra",&ed::newrefarch);

}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork {namespace tool {
