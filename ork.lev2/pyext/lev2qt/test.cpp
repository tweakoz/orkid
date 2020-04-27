#include "test.h"
#include <ork/pch.h>
#include <QtCore/QCoreApplication>
////////////////////////////////////////////////////////////////////////////////
// undefine qApp, which was previously defined as QCoreApplication::instance()
#undef qApp
QCoreApplication* qApp = QCoreApplication::instance();
////////////////////////////////////////////////////////////////////////////////
mytype::mytype(const std::string& inp)
    : _val(inp) {
}
////////////////////////////////////////////////////////////////////////////////
Icecream::Icecream(const mytype& flavor)
    : _theflavor(flavor) {

  int on_stack = 0;
  int* on_heap = new int(5);
  printf("addr-this <%p>\n", this);
  printf("addr-flavor <%p>\n", &flavor);
  printf("addr-_theflavor <%p>\n", &_theflavor);
  printf("addr-on_stack <%p>\n", &on_stack);
  printf("addr-on_heap <%p>\n", on_heap);
  printf("Whats your flavor <%s>\n", flavor._val.c_str());
  printf("Whats your flavor <%s>\n", _theflavor._val.c_str());
  // OrkAssert(typeid(flavor) == typeid(_theflavor));
  _theflavor = flavor;
  printf("Whats your flavor <%s>\n", _theflavor._val.c_str());

  mytype newflavor = {"wtf"};
  printf("Whats your newflavor <%s>\n", newflavor._val.c_str());
  // OrkAssert(false);
}
////////////////////////////////////////////////////////////////////////////////
const mytype& Icecream::getFlavor() const {
  return _theflavor;
}
////////////////////////////////////////////////////////////////////////////////
