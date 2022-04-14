////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

import ork_corex;

using namespace ork;

int main(int argc, char** argv){
  auto X = "ORKMODULE"_crc;

  printf( "ORKMODULE<%zx>\n", X._hashed );
	//MTEST_MODULE();
	return 0;
}