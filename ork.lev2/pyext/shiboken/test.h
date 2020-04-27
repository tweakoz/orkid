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
  virtual ~mytype() {
  }
  std::string _val;
};

class Icecream {
public:
  Icecream(const mytype& flavor);
  virtual ~Icecream() {
  }
  const mytype& getFlavor() const;

private:
  mytype _theflavor;
};
