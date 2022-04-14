////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/future.hpp>

using namespace ork;

struct fut_yo
{

};

TEST(OrkFuture)
{
	printf( "futtest\n" );

/*
 * auto l_thread = [](anyp data)({
    	Future* pfut = static_cast<Future*>(ctx);
	    usleep(1<<20);
    	pfut->Signal(fut_yo());
	};

	ork::Thread thr_p(l_thread);

	TrackCurrent ot(nullptr);

    Opq the_opq(1);

	Future the_future;
	ork::atomic<int> gcounter;
	gcounter = 0;
	for( int i=0; i<100; i++ )
	{
		auto lam = [&]()
		{
			gcounter++;
		};
		the_opq.push_sync(Op(lam));
	}
	printf( "test:OrkFuture gcounter<%d>\n", int(gcounter) );
*/
}
