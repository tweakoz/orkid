#include <unittest++/UnitTest++.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/future.hpp>

using namespace ork;

struct fut_yo
{

};

static void* future_thread(void* ctx)
{
	Future* pfut = static_cast<Future*>(ctx);
	usleep(1<<20);
	pfut->Signal(fut_yo());
	return 0;
}
TEST(OrkFuture)
{
	printf( "futtest\n" );

	OpqTest ot(nullptr);

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

}
