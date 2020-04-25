#pragma once
#include <string>
#include <ork/orkconfig.h>
#include <QtCore/QString>

class ORK_API Icecream {
public:
  Icecream(const char* flavor);
  const std::string& getFlavor() const;

private:
  std::string _theflavor;
};
