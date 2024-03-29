////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/file/path.h>
#include <ork/kernel/varmap.inl>

namespace ork { namespace lev2 {
class CUIWidget;
}} // namespace ork::lev2

namespace ork {
///////////////////////////////////////////////////////////////////////////////

enum EMemType { EMEMTYPE_AUD = 0, EMEMTYPE_GFX, EMEMTYPE_SYS, EMEMTYPE_END };

class File;

///////////////////////////////////////////////////////////////////////////////

struct OldSchool // because this is like the oldest code in orkid i think..
    : public NoRttiSingleton<OldSchool> {
public: //
  OldSchool();
  ~OldSchool();

  static int GetNumCores();
  ////////////////////////////////////////////////////////

  f64 mfClockRate;

#if defined(_WIN32)

  double li2d(LARGE_INTEGER i) {
    double d;
    d = i.LowPart + (i.HighPart * 4294967296.0);
    return (d);
  }

  // return double difference from two large integer
  double get_diff(LARGE_INTEGER i1, LARGE_INTEGER i2) {
    double d1, d2, diff;
    d1   = li2d(i1);
    d2   = li2d(i2);
    diff = d2 - d1;
    return (diff);
  }

#endif

  f32 GetLoResTime(void);
  f32 GetLoResRelTime(void);

  f64 GetHiResTime(void);
  f64 GetHiResRelTime(void);

  static S64 GetClockCycle(void);
  static S64 ClockCyclesToMicroSeconds(S64 cycles);

  ////////////////////////////////////////////////////////

  int SpawnProcess(const std::string& exename, const std::string& args);

  ////////////////////////////////////////////////////////

  typedef enum { LOG_SEVERITY_INFO = 0, LOG_SEVERITY_WARN, LOG_SEVERITY_ERROR, LOG_SEVERITY_FATAL } LOG_SEVERITY;

  struct LogPolicy : public ork::util::Context<LogPolicy> {
    LogPolicy()
        : mFileOut(false)
        , mAllChannelsToStdOut(false) {
    }
    bool mFileOut;
    bool mAllChannelsToStdOut;
    orkset<std::string> mChannelsToStdOut;
  };

  static void Log(LOG_SEVERITY severity, const std::string& chanid, char* formatstring, ...);

  ////////////////////////////////////////////////////////

  static void SetGlobalPathVariable(const std::string& variable, file::Path value);
  static void SetGlobalStringVariable(const std::string& variable, std::string value);
  static void SetGlobalIntVariable(const std::string& variable, int value);
  static void SetGlobalFloatVariable(const std::string& variable, f32 value);
  static file::Path GetGlobalPathVariable(const std::string& variable);
  static std::string GetGlobalStringVariable(const std::string& variable);
  static int GetGlobalIntVariable(const std::string& variable);
  static f32 GetGlobalFloatVariable(const std::string& variable);

  ////////////////////////////////////////////////////////

  static bool IsKeyDepressed(int ch);

  ////////////////////////////////////////////////////////

  static const char* ExpandString(char* outbuf, size_t outsize, const char* pfmtstr);

  ////////////////////////////////////////////////////////

  int miSubTime;

private:
  U32 muGlobBufMap;

  S64 miBaseCycle;
  f64 mfWallClockBaseTime;
  f64 mfWallClockTime;
  int miCalibCounter;
  U32 muMemTypeFlags[EMEMTYPE_END];
  orkmap<std::string, File*> mvLogChannels;

  varmap::TVarMap<svar512_t> _variables;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
