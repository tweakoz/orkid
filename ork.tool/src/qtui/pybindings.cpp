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


///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////
ent::SceneData* PyNewScene();
ent::SceneData* PyGetScene();
void PyNewRefArch(const std::string& name);
void PyNewArch(const std::string& classname, const std::string& name);
void PyNewEntity(const std::string& name, const std::string& archname = "");
///////////////////////////////////////////////////////////////////////////////

class ed {
public:
  std::string whatup() {
    return std::string("whatup yourself");
  }
  std::string damn() {
    return std::string("hot damn");
  }
  ent::SceneData* getscene() {
    return PyGetScene();
  }
  ent::SceneData* newscene() {
    return PyNewScene();
  }
  void newentity(const std::string& entname, const std::string& archname = "no_arch") {
    PyNewEntity(entname, archname);
  }
  void newrefarch(const std::string& name) {
    PyNewRefArch(name);
  }
  void newarch(const std::string& classname, const std::string& name) {
    PyNewArch(classname, name);
  }
};

///////////////////////////////////////////////////////////////////////////////

void orkpy_initork() {
  ////////////////////////
  // ork::fvec3
  ////////////////////////

  py::module mm("__main__", "Orkid");
  py::object main_namespace = mm.attr("__dict__");

  main_namespace["scene"] = py::class_<ent::SceneData>(mm, "Scene")
                                .def("objects", [](ent::SceneData* sd) -> std::list<std::pair<std::string, ork::Object*>> {
                                  std::list<std::pair<std::string, ork::Object*>> rval;
                                  auto& objs = sd->GetSceneObjects();
                                  for (const auto& item : objs) {
                                    auto name = item.first.c_str();
                                    auto obj  = (ork::Object*)item.second;
                                    auto p    = std::pair<std::string, ork::Object*>(name, obj);
                                    rval.push_back(p);
                                  }

                                  return rval;
                                });

  ////////////////////////
  // scene editor
  ////////////////////////

  main_namespace["editor"] = py::class_<ed>(mm, "editor")
                                 .def(py::init<>())
                                 .def("whatup", &ed::whatup)
                                 .def("damn", &ed::damn)
                                 .def("newscene", &ed::newscene)
                                 .def("newentity", &ed::newentity)
                                 .def("newrefarch", &ed::newrefarch)
                                 .def("newarch", &ed::newarch)
                                 .def("s", &ed::getscene)
                                 .def("ns", &ed::newscene)
                                 .def("ne", &ed::newentity)
                                 .def("na", &ed::newarch)
                                 .def("nra", &ed::newrefarch);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
