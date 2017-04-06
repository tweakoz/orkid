#include <python.h>
#include <boost/python.hpp>
#include <boost/python/str.hpp>
#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
using namespace boost::python;
namespace bpy = boost::python;
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
};

///////////////////////////////////////////////////////////////////////////////

void orkpy_initork()
{
    ////////////////////////
    // ork::CVector3
    ////////////////////////

    bpy::object main_module((bpy::handle<>(bpy::borrowed(PyImport_AddModule("__main__")))));
    bpy::object main_namespace = main_module.attr("__dict__");

    main_namespace["vec2"] = bpy::class_<CVector2>("vec2")
                            .add_property("x", &CVector2::GetX, &CVector2::SetX)
                            .add_property("y", &CVector2::GetY, &CVector2::SetY)
                            .def("dot",&CVector2::Dot) // __add__
                            .def("perp",&CVector2::PerpDot) // __add__
                            .def("mag",&CVector2::Mag) // __add__
                            .def("magsquared",&CVector2::MagSquared) // __add__
                            .def("lerp",&CVector2::Lerp) // __add__
                            .def("serp",&CVector2::Serp) // __add__
                            .def("normal",&CVector2::Normal) // __add__
                            .def("normalize",&CVector2::Normalize) // __add__
                            .def(self + self) // __add__
                            .def(self - self) // __sub__
                            .def(self * self) // __scalar mul__
                            .def(self_ns::str(self)); 

    main_namespace["vec3"] = bpy::class_<CVector3>("vec3")
                            .add_property("x", &CVector3::GetX, &CVector3::SetX)
                            .add_property("y", &CVector3::GetY, &CVector3::SetY)
                            .add_property("z", &CVector3::GetZ, &CVector3::SetZ)
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
                            .def(self + self) // __add__
                            .def(self - self) // __sub__
                            .def(self * self) // __scalar mul__
                            .def(self_ns::str(self)); 
                            
    ////////////////////////
    // scene editor
    ////////////////////////

    main_namespace["editor"] = bpy::class_<ed>("editor")
                            .def("whatup",&ed::whatup)      
                            .def("damn",&ed::damn)      
                            .def("newscene",&ed::newscene)      
                            .def("newentity",&ed::newentity)
                            .def("newrefarch",&ed::newrefarch)
                            .def("ns",&ed::newscene)    
                            .def("ne",&ed::newentity)
                            .def("nra",&ed::newrefarch);

}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork {namespace tool {
