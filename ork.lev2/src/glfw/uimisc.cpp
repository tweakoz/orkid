///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>

#if defined(ORK_CONFIG_IX)
#include <cxxabi.h>
#endif

namespace ork {

file::Path SaveFileRequester(const std::string& title, const std::string& ext) {
  /*auto& gfxenv = lev2::GfxEnv::GetRef();
  gfxenv.GetGlobalLock().Lock();
  QString FileName           = QFileDialog::getSaveFileName(0, "Export ProcTexImage", 0, "PNG (*.png)");
  file::Path::NameType fname = FileName.toStdString().c_str();
  gfxenv.GetGlobalLock().UnLock();
  return file::Path(fname);
  */
  return file::Path();
}

#if defined(ORK_CONFIG_IX)
std::string GccDemangle(const std::string& inname) {
  int status;
  const char* pmangle = abi::__cxa_demangle(inname.c_str(), 0, 0, &status);
  return std::string(pmangle);
}
#endif

std::string TypeIdNameStrip(const char* name) {
  std::string strippedName(name);

#if defined(ORK_CONFIG_IX)
  strippedName = GccDemangle(strippedName);
#endif

  size_t classLength = strlen("class ");
  ;
  size_t classPosition;
  while ((classPosition = strippedName.find("class ")) != std::string::npos) {
    strippedName.swap(strippedName.erase(classPosition, classLength));
  }

  return strippedName;
}
std::string MethodIdNameStrip(const char* name) // mainly used for QT signals and slots
{

  std::string inname(name);

#if defined(ORK_CONFIG_IX)
  inname = GccDemangle(inname);
#endif

  FixedString<65536> newname = inname.c_str();
  newname.replace_in_place("std::", "");
  newname.replace_in_place("__thiscall ", "");
  return newname.c_str();
}

std::string TypeIdName(const std::type_info* ti) {
  return (ti != nullptr) ? TypeIdNameStrip(ti->name()) : "nil";
}

namespace lev2 {

fvec2 logicalMousePos() {
  fvec2 rval; // current mouse pos
  if (_HIDPI()) {
    rval.x *= 0.5f;
    rval.y *= 0.5f;
  }
  return rval;
}

void OrkGlobalDisableMousePointer() {
  //QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
}
void OrkGlobalEnableMousePointer() {
  //QApplication::restoreOverrideCursor();
}

} // namespace lev2
} // namespace ork
