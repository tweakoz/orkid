#include <unittest++/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/kernel/ringbuffer.hpp>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/opq.h>

using namespace ork;

TEST(opq_maxinflight)
{
    float fsynctime = ork::get_sync_time();

    srand( u32(fsynctime*100.0f) ); // so we dont get the same thread count seqeuence every run

    printf( "sizeof(OpGroup) %d\n", int(sizeof(OpGroup)));
    printf( "sizeof(void_lambda_t) %d\n", int(sizeof(void_lambda_t)));

    for( int i=0; i<4; i++ )
    {
        const int knumthreads = 1 + rand()%64;
        const int kopqconcurr = knumthreads;
        const int kops = knumthreads*1000;

        auto gopq1 = new Opq(knumthreads);
        auto gopg1 = gopq1->CreateOpGroup("g1");
        gopg1->mLimitMaxOpsInFlight = kopqconcurr;

        ork::atomic<int> ops_in_flight;
        ork::atomic<int> counter;
        ops_in_flight = 0;
        counter = 0;

        //////////////////////////////////////

    	void_lambda_t lam = [&]()
    	{
            ops_in_flight++;

            assert(int(ops_in_flight)<=kopqconcurr);
            assert(int(ops_in_flight)>=0);

            int ir = 64+(rand()%64);
            float fr = float(ir)*0.0000001f;

            ork::Timer throttle;
            throttle.Start();

            //printf( "BEG Op<%p> wait<%dms> OIF<%d>\n", this, ir, int(ops_in_flight) );

            int i =0;

            while(throttle.SecsSinceStart()<fr)
            {
                i++;
            }

            ops_in_flight--;

            counter++;       		
    	};

        //////////////////////////////////////

        ork::Timer measure;
        measure.Start();

        for( int i=0; i<kops; i++ )
        {
            gopg1->push(Op(lam,"yo"));
        }

        gopq1->drain();

        CHECK(counter==kops);

        float elapsed = measure.SecsSinceStart();

        float fopspsec = float(kops)/elapsed;

        printf( "opqmaxinflight test nthr<%d> elapsed<%.02f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d>\n", knumthreads, elapsed, kops, int(fopspsec), int(fopspsec/float(knumthreads)), int(counter) );

    	delete gopq1;
    }
}

///////////////////////////////////////////////////////////////////////////////
