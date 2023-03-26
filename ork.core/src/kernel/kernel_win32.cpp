////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/orkconfig.h>

#include <ork/file/file.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////

using ork::FileEnv;

#if defined(_WIN32) && !defined(_XBOX)
#include <psapi.h>

std::string CurrentExeName(void) {
  OrkAssertNotImplI("Refactor to DLL will make this not link");

  char ExeNameBuffer[256];
  BOOL b = GetModuleFileNameEx(GetCurrentProcess(), NULL, ExeNameBuffer, 256);
  std::string ret = ExeNameBuffer;

  return ret;
}

std::string getExeName(std::string strFullPathToExe) {
  size_t Position = strFullPathToExe.find_last_of("\\");
  strFullPathToExe = strFullPathToExe.erase(0, Position + 1);

  return strFullPathToExe;
}

int ExecuteProcess(std::string FullPathToExe, std::string Parameters, int SecondsToWait) {
  int iMyCounter = 0, iReturnVal = 0;

  /* - NOTE - You could check here to see if the exe even exists */

  /* Add a space to the beginning of the Parameters */
  if (Parameters.size() != 0) {
    Parameters.insert(0, " ");
  }

  /* When using CreateProcess the first parameter needs to be the exe itself */
  Parameters = FullPathToExe + Parameters;

  /*
      The second parameter to CreateProcess can not be anything but a char !!
      Since we are wrapping this C function with strings, we will create
      the needed memory to hold the parameters
  */

  /* Dynamic Char */
  char* pszParam = new char[Parameters.size() + 1];

  /* Verify memory availability */
  if (pszParam == 0) {
    /* Unable to obtain (allocate) memory */
    return 1;
  }
  const char* pchrTemp = Parameters.c_str();
  strcpy(pszParam, pchrTemp);

  ////////////////////////////////////////////
  // Startup Directory
  ////////////////////////////////////////////

  const ork::file::Path::NameType& WorkingFolder = ork::file::GetStartupDirectory();
  ork::file::Path WorkingFolderPath = ork::file::Path(WorkingFolder.c_str()).ToAbsolute();

  /* CreateProcess API initialization */
  STARTUPINFO siStartupInfo;
  PROCESS_INFORMATION piProcessInfo;
  memset(&siStartupInfo, 0, sizeof(siStartupInfo));
  memset(&piProcessInfo, 0, sizeof(piProcessInfo));
  siStartupInfo.cb = sizeof(siStartupInfo);

  // Execute
  if (CreateProcess(FullPathToExe.c_str(),     // ApplicationName
                    pszParam,                  // CommandLine
                    0,                         // lpProcessAttributes
                    0,                         // lpThreadAttributes
                    false,                     // InheritHandles
                    CREATE_DEFAULT_ERROR_MODE, // CreationFlags
                    0,                         // Environment
                    WorkingFolderPath.c_str(), // Current Directory
                    &siStartupInfo,            // StartupInfo
                    &piProcessInfo             // ProcessInfo
                    ) != false) {
    if (SecondsToWait == -1)
      return 0;

    WaitForSingleObject(piProcessInfo.hProcess, INFINITE);
  } else {
    // CreateProcess failed. You could also set the return to GetLastError()
    iReturnVal = 2;
  }

  // Release handles
  CloseHandle(piProcessInfo.hProcess);
  CloseHandle(piProcessInfo.hThread);

  // Free memory
  delete[] pszParam;
  pszParam = 0;

  return iReturnVal;
}

namespace ork {

int System::SpawnProcess(const std::string& exename, const std::string& args) {
  int rv = ExecuteProcess(exename, args, 0);

  return rv;
}

} // namespace ork

#endif
