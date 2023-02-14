////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <assert.h>
#include <string.h>

namespace ork {

Environment genviron;

#if defined(__APPLE__)
extern char** environ;
#endif

///////////////////////////////////////////////////////////////////////////////
// environment variable utils
///////////////////////////////////////////////////////////////////////////////

Environment::Environment()
{

}

///////////////////////////////////////////////////////////////////////////////

void Environment::init_from_envp(char** envp)
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

}

///////////////////////////////////////////////////////////////////////////////

void Environment::set( const std::string& k, const std::string& v )
{
    mEnvMap[k] = v;
    int ok = setenv(k.c_str(),v.c_str(),1/*overwrite*/);
    OrkAssert(ok==0);
}

///////////////////////////////////////////////////////////////////////////////

void Environment::appendPath( const std::string& k, const file::Path& v ) {
  auto it = mEnvMap.find(k);
  if( it == mEnvMap.end() ){
      set(k,v.toAbsolute().toStdString());
  }
  else {
    auto prev_val = it->second;
    set(k,prev_val+":"+v.toAbsolute().toStdString());
  }
}

///////////////////////////////////////////////////////////////////////////////

void Environment::prependPath( const std::string& k, const file::Path& v ) {
  auto it = mEnvMap.find(k);
  if( it == mEnvMap.end() ){
      set(k,v.toStdString());
  }
  else {
    auto prev_val = it->second;
    set(k,v.toStdString()+":"+prev_val);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Environment::has( const std::string& k ) const
{
    auto it = mEnvMap.find(k);
    return it!=mEnvMap.end();
}

///////////////////////////////////////////////////////////////////////////////

bool Environment::get( const std::string& k, std::string& vout ) const
{
    auto it = mEnvMap.find(k);
    bool brval = it!=mEnvMap.end();
    if( brval )
        vout = it->second;
    return brval;

}

///////////////////////////////////////////////////////////////////////////////

void Environment::dump() const
{
    for( const auto& item : mEnvMap )
    {
        printf( "KEY<%s> VAL<%s>\n", item.first.c_str(), item.second.c_str() );
    }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
