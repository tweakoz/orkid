////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/util/choiceman.h>
#include <orktool/toolcore/selection.h>

#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/string/string.h>

#include <stdio.h>
#include <sys/stat.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/string/string.h>
///////////////////////////////////////////////////////////////////////////////
#include <iterator>
//#include <boost/algorithm/string.hpp>
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
const std::string& getDataDir();
const std::string& getExecutableDir();
void setDataDir(const std::string& dir);

int Main_Filter(tokenlist toklist);
int Main_FilterTree(tokenlist toklist);
int QtTest(int& argc, char** argv, bool bgamemode, bool bmenumode);
tokenlist Init(int& argc, char** argv, char** argp);
}} // namespace ork::tool
