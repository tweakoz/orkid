#include "test.h"

class QApplication;
QApplication* qApp = nullptr;

#include <lev2-qtui/Universe/universe_module_wrapper.cpp>

#include <QApplication>
// extern "C" SBK_EXPORT_MODULE PyObject* PyInit_Unoverse()

namespace ork::lev2 {
PyObject* get_universe() {
  return PyInit_Universe();
}
} // namespace ork::lev2
