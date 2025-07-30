////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <assert.h>
#include <string.h>
#include <unordered_set>

// #if defined(__APPLE__)
extern char** environ;
// #endif

namespace ork {

Environment genviron;

///////////////////////////////////////////////////////////////////////////////
// environment variable utils
///////////////////////////////////////////////////////////////////////////////

Environment::Environment() {
  init_from_global_env();
}

///////////////////////////////////////////////////////////////////////////////

void Environment::init_from_global_env() {
  for (char** env = environ; *env != 0; env++) {
    char* this_env = *env;

    if (this_env) {
      std::string estr(this_env);
      const char* pbeg = estr.c_str();
      const char* peq  = strstr(pbeg, "=");
      assert(peq[0] == '=');
      size_t klen     = peq - pbeg;
      std::string key = estr.substr(0, klen);
      std::string val = estr.substr(klen + 1, estr.length());
      mEnvMap[key]    = val;
      // printf( "split<%s> k<%s> v<%s>\n", peq, key.c_str(), val.c_str() );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

/*void Environment::init_from_envp(char** envp)
{
    for( char** env=envp; *env!=0; env++ )
    {
       char* this_env = *env;

        if( this_env )
        {
            std::string estr(this_env);
            const char* pbeg = estr.c_str();
            const char* peq = strstr(pbeg,"=");
            assert(peq[0]=='=');
            size_t klen = peq-pbeg;
            std::string key = estr.substr(0,klen);
            std::string val = estr.substr(klen+1,estr.length());
            mEnvMap[key] = val;
            //printf( "split<%s> k<%s> v<%s>\n", peq, key.c_str(), val.c_str() );
        }
    }

}*/

///////////////////////////////////////////////////////////////////////////////

void Environment::set(const std::string& k, const std::string& v) {
  mEnvMap[k] = v;
  int ok     = setenv(k.c_str(), v.c_str(), 1 /*overwrite*/);
  OrkAssert(ok == 0);
}

///////////////////////////////////////////////////////////////////////////////

void Environment::appendPath(const std::string& k, const file::Path& v) {
  auto it = mEnvMap.find(k);
  if (it == mEnvMap.end()) {
    set(k, v.toAbsolute().toStdString());
  } else {
    auto prev_val = it->second;
    set(k, prev_val + ":" + v.toAbsolute().toStdString());
  }
}

///////////////////////////////////////////////////////////////////////////////

void Environment::prependPath(const std::string& k, const file::Path& v) {
  auto it = mEnvMap.find(k);
  if (it == mEnvMap.end()) {
    set(k, v.toStdString());
  } else {
    auto prev_val = it->second;
    set(k, v.toStdString() + ":" + prev_val);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Environment::has(const std::string& k) const {
  auto it = mEnvMap.find(k);
  return it != mEnvMap.end();
}

///////////////////////////////////////////////////////////////////////////////

bool Environment::get(const std::string& k, std::string& vout) const {
  auto it    = mEnvMap.find(k);
  bool brval = it != mEnvMap.end();
  if (brval)
    vout = it->second;
  return brval;
}

///////////////////////////////////////////////////////////////////////////////

void Environment::dump() const {
  for (const auto& item : mEnvMap) {
    printf("KEY<%s> VAL<%s>\n", item.first.c_str(), item.second.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork

///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
#include <crt_externs.h>
std::vector<std::string> get_args() {
  std::vector<std::string> args;
  char** argv = *_NSGetArgv();
  int argc    = *_NSGetArgc();
  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return args;
}
#else
#include <unistd.h>
#include <fcntl.h>

std::vector<std::string> get_args() {
  std::vector<std::string> args;
  char buf[4096];
  char* _buf = buf;
  int fd     = open("/proc/self/cmdline", O_RDONLY);
  if (fd >= 0) {
    ssize_t len = read(fd, buf, sizeof(buf));
    close(fd);
    // Args are null-separated in buf
    for (ssize_t i = 0; i < len; ++i) {
      if (_buf[i] == '\0') {
        args.push_back(std::string(_buf, i));
        _buf += (i + 1); // Move past null terminator
      }
    }
  }
  return args;
}
#endif

std::unordered_set<std::string> get_args_set() {
  auto args = get_args();
  return std::unordered_set<std::string>(args.begin(), args.end());
}
