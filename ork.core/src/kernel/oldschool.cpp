////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/core_interface.h>
#include <ork/kernel/opq.h>

//////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX)
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(ORK_CONFIG_IX)
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <time.h>
#endif
//////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/kernel.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <cmath>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

float get_sync_time();

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

//////////////////////////////////////////////////////////////////////////////

int OldSchool::GetNumCores()
{
#if defined(ORK_CONFIG_IX)
  int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
#else
  static int numCPUs = -1;
  if(-1 == numCPUs ){
    size_t count_len = sizeof(numCPUs);
   sysctlbyname("hw.logicalcpu", &numCPUs, &count_len, NULL, 0);

  //fprintf(stderr,"you have %i cpu cores\n", numCPUs);
  fflush(stdout);
}
#endif

  const char* numcores_env = getenv("OBT_NUM_CORES");
  if(numcores_env){
    numCPUs = atoi(numcores_env);
  }

  return numCPUs;

}

//////////////////////////////////////////////////////////////////////////////

S64 OldSchool::GetClockCycle(void) {
  uint64_t counter = 0; 
  
  #if defined(ORK_ARCHITECTURE_X86_64)
  counter = __builtin_readcyclecounter();
  #elif defined(ORK_ARCHITECTURE_ARM_64)
  __asm __volatile("mrs %0, CNTVCT_EL0" : "=&r" (counter));
  #else
  #error // not implemented
  #endif
    
    return S64(counter);
}

//////////////////////////////////////////////////////////////////////////////

S64 OldSchool::ClockCyclesToMicroSeconds(S64 cycles)
{
  OrkAssert(false);//not impl
  return S64(0);
}

///////////////////////////////////////////////////////////////////////////////
// lower res higher reliability timing
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////
#if defined( __APPLE__ )
///////////////////////////////////////////////
f32 OldSchool::GetLoResTime( void )
{
  auto getres = []->double
  {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    return (double)timebase.numer / (double)timebase.denom / 1000000.0;
  };
  static double resolution = getres();
    static uint64_t gmachtime_ref = mach_absolute_time();
    uint64_t machtime_cur = mach_absolute_time();
    double millis = double(machtime_cur-gmachtime_ref) * resolution;
  return float(millis*0.001);
}
///////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////
f32 OldSchool::GetLoResTime( void )
{
  static const float kbasetime = get_sync_time();
  return get_sync_time()-kbasetime;
}
#endif
///////////////////////////////////////////////////////////////////////////////
f32 OldSchool::GetLoResRelTime( void )
{
    static f32 fBaseTime = GetLoResTime();
    f32 fCurTime = GetLoResTime();
    f32 fRelTime = fCurTime-fBaseTime;
    return fRelTime;
}
///////////////////////////////////////////////////////////////////////////////
// higher res lower reliability timing
///////////////////////////////////////////////////////////////////////////////

f64 OldSchool::GetHiResRelTime( void )
{
    static f64 fBaseTime = GetHiResTime();
    f64 fCurTime = GetHiResTime();
    f64 fRelTime = fCurTime-fBaseTime;

    return fRelTime;
}

f64 OldSchool::GetHiResTime( void )
{
    f64 fTime = 0.0f;
    mfWallClockTime = fTime;
    return fTime;
}

} // namespace ork
