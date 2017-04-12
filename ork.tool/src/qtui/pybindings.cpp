#include <python.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/fixedstring.hpp>
#include <ostream>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

namespace std
{
    ostream& operator<<(ostream& ostr,const ::ork::CVector3& vec3)
    {
        ostr<<"("<<vec3.GetX()<<","<<vec3.GetY()<<","<<vec3.GetZ()<<")";
        return ostr;
    }
    ostream& operator<<(ostream& ostr,const ::ork::CVector2& vec2)
    {
        ostr<<"("<<vec2.GetX()<<","<<vec2.GetY()<<")";
        return ostr;
    }
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////
void PyNewScene();
void PyNewRefArch(const std::string& name);
void PyNewArch(const std::string& name);
void PyNewEntity(const std::string& name,const std::string& archname="");
///////////////////////////////////////////////////////////////////////////////

class ed 
{
public:
    std::string whatup()
    {
        return std::string( "whatup yourself" );
    }
    std::string damn()
    {
        return std::string( "hot damn" );
    }
    void newscene()
    {
        PyNewScene();
    }
    void newentity(const std::string&entname,const std::string&archname="no_arch")
    {
        PyNewEntity(entname,archname);
    }
    void newrefarch(const std::string&name)
    {
        PyNewRefArch(name);
    }
    void newarch(const std::string&name)
    {
        PyNewArch(name);
    }
};

///////////////////////////////////////////////////////////////////////////////

void orkpy_initork()
{
    ////////////////////////
    // ork::CVector3
    ////////////////////////

    py::module mm("__main__","Orkid");
    py::object main_namespace = mm.attr("__dict__");

    main_namespace["vec2"] = py::class_<CVector2>(mm,"vec2")
                            .def(py::init<>())
                            .def(py::init<float,float>())
                            .def_property("x", &CVector2::GetX, &CVector2::SetX)
                            .def_property("y", &CVector2::GetY, &CVector2::SetY)
                            .def("dot",&CVector2::Dot) // __add__
                            .def("perp",&CVector2::PerpDot) // __add__
                            .def("mag",&CVector2::Mag) // __add__
                            .def("magsquared",&CVector2::MagSquared) // __add__
                            .def("lerp",&CVector2::Lerp) // __add__
                            .def("serp",&CVector2::Serp) // __add__
                            .def("normal",&CVector2::Normal) // __add__
                            .def("normalize",&CVector2::Normalize) // __add__
                            .def(py::self + py::self) // __add__
                            .def(py::self - py::self) // __sub__
                            .def(py::self * py::self) // __scalar mul__
                            .def("__str__", [](const CVector2& v)->std::string
                            {
                                fxstring<64> fxs; fxs.format("vec2(%g,%g)",v.x,v.y);
                                return fxs.c_str();
                            });
                            //.def(self_ns::str(self)); 

    main_namespace["vec3"] = py::class_<CVector3>(mm,"vec3")
                            .def(py::init<>())
                            .def(py::init<float,float,float>())
                            .def_property("x", &CVector3::GetX, &CVector3::SetX)
                            .def_property("y", &CVector3::GetY, &CVector3::SetY)
                            .def_property("z", &CVector3::GetZ, &CVector3::SetZ)
                            .def("dot",&CVector3::Dot) // __add__
                            .def("cross",&CVector3::Cross) // __add__
                            .def("mag",&CVector3::Mag) // __add__
                            .def("magsquared",&CVector3::MagSquared) // __add__
                            .def("lerp",&CVector3::Lerp) // __add__
                            .def("serp",&CVector3::Serp) // __add__
                            .def("reflect",&CVector3::Reflect) // __add__
                            .def("saturate",&CVector3::Saturate) // __add__
                            .def("normal",&CVector3::Normal) // __add__
                            .def("normalize",&CVector3::Normalize) // __add__
                            .def("rotx",&CVector3::RotateX) // __add__
                            .def("roty",&CVector3::RotateY) // __add__
                            .def("rotz",&CVector3::RotateZ) // __add__
                            .def(py::self + py::self) // __add__
                            .def(py::self - py::self) // __sub__
                            .def(py::self * py::self)// // __scalar mul__
                            .def("__str__", [](const CVector3& v)->std::string
                            {
                                fxstring<64> fxs; fxs.format("vec3(%g,%g,%g)",v.x,v.y,v.z);
                                return fxs.c_str();
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
                            .def("ns",&ed::newscene)    
                            .def("ne",&ed::newentity)
                            .def("na",&ed::newarch)
                            .def("nra",&ed::newrefarch);

}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork {namespace tool {
