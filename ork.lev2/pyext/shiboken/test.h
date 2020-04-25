#pragma once
#include <string>
#include <ork/orkconfig.h>
#include <QtCore/QCoreApplication>
#undef qApp
extern QCoreApplication* qApp;
//#include <ork/math/cvector2.h>

class mytype {
public:
  mytype(const char* inp);
  std::string _val;
};

class Icecream {
public:
  Icecream(const mytype& flavor);
  const mytype& getFlavor() const;

private:
  mytype _theflavor;
};
