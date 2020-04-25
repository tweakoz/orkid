#pragma once
#include <string>
#include <ork/orkconfig.h>
//#include <ork/math/cvector2.h>
//#include <QtCore/QString>

class Icecream {
public:
  Icecream(const char* flavor);
  const std::string& getFlavor() const;

private:
  std::string _theflavor;
};
