#include "test.h"

class QApplication;
static QApplication* qApp = nullptr;

#include <lev2-qtui/_lev2qt/_lev2qt_module_wrapper.cpp>

#include <ork/lev2/ezapp.h>
// extern "C" SBK_EXPORT_MODULE PyObject* PyInit_Universe()

namespace ork::lev2 {
PyObject* get_universe() {
  static std::string arg = "yo";
  static char* argv[1]   = {(char*)arg.c_str()};
  static int argc        = 1;
  static auto app        = std::make_shared<OrkEzQtAppBase>(argc, argv);
  // qApp            = OrkEzQtAppBase::get();
  return PyInit__lev2qt();
}
} // namespace ork::lev2
