////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>

#include <ork/kernel/core_interface.h>
#include <ork/kernel/opq.h>

const std::string gstring_noval("");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

void OldSchool::SetGlobalFloatVariable(const std::string& variable, f32 value) {
  GetRef()._variables.makeValueForKey<float>(variable) = value;
}

f32 OldSchool::GetGlobalFloatVariable(const std::string& variable) {
  return GetRef()._variables.typedValueForKey<float>(variable).value();
}

///////////////////////////////////////////////////////////////////////////////

void OldSchool::SetGlobalStringVariable(const std::string& variable, std::string value) {
  GetRef()._variables.makeValueForKey<std::string>(variable) = value;
}

std::string OldSchool::GetGlobalStringVariable(const std::string& variable) {
  return GetRef()._variables.typedValueForKey<std::string>(variable).value();
}

///////////////////////////////////////////////////////////////////////////////

void OldSchool::SetGlobalIntVariable(const std::string& variable, int value) {
  GetRef()._variables.makeValueForKey<int>(variable) = value;
}

int OldSchool::GetGlobalIntVariable(const std::string& variable) {
  return GetRef()._variables.typedValueForKey<int>(variable).value();
}

///////////////////////////////////////////////////////////////////////////////

void OldSchool::SetGlobalPathVariable(const std::string& variable, file::Path value) {
  GetRef()._variables.makeValueForKey<file::Path>(variable) = value;
}

file::Path OldSchool::GetGlobalPathVariable(const std::string& variable) {
  return GetRef()._variables.typedValueForKey<file::Path>(variable).value();
}

///////////////////////////////////////////////////////////////////////////////

OldSchool::OldSchool()
    : NoRttiSingleton<OldSchool>() {
#if defined(NITRO)
  OS_InitTick();
#endif

  miCalibCounter      = 0;
  miBaseCycle         = GetClockCycle();
  mfWallClockBaseTime = GetHiResTime();
}

///////////////////////////////////////////////////////////////////////////////

const char* OldSchool::ExpandString(char* outbuf, size_t outsize, const char* pfmtstr) {
  size_t out = 0;
  char keybuf[512];
  //////////////////////////////
  // TODO make this generic so for every $(x) variable it replaces with GetGlobalStringVariable(x)

  for (intptr_t in = 0; pfmtstr[in];) {
    if (pfmtstr[in] == '$' && pfmtstr[in + 1] == '(') {
      in += 2;

      const char* endparen = strchr(&pfmtstr[in], ')');

      if (endparen == NULL) {
        OrkAssertI(0, pfmtstr);
      }

      intptr_t keylen = endparen - &pfmtstr[in];

      OrkAssert(keylen < sizeof(keybuf) - 1);
      strncpy(keybuf, &pfmtstr[in], size_t(keylen));
      OrkAssert(keylen < sizeof(keybuf));
      keybuf[keylen] = '\0';

      in += keylen + 1;

      std::string value = GetGlobalStringVariable(keybuf);

      for (std::string::size_type v = 0; v < value.size(); v++) {
        if (out < outsize - 1)
          outbuf[out++] = value[v];
      }
    } else {
      outbuf[out++] = pfmtstr[in++];
    }
  }

  OrkAssert(out < outsize);
  outbuf[out++] = '\0';
  return outbuf;
}

} // namespace ork
