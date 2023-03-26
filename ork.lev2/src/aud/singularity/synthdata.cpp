////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::BankData, "SynBankData");

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////

void BankData::describeX(class_t* clazz) {
  clazz->directObjectMapProperty("Programs", &BankData::_programsByName);
  // clazz->directObjectMapProperty("KeyMaps", &BankData::_keymaps);
}

//////////////////////////////////////////////////////////////////////////////

void BankData::addProgram(int idx, const std::string& name, prgdata_ptr_t program) {
  _programs[idx]        = program;
  _programsByName[name] = program;
}

file::Path basePath() {
  return file::Path::share_dir() / "singularity";
}

float SynthData::seqTime(float dur) {
  float rval = _seqCursor;
  _seqCursor += dur;
  return rval;
}

prgdata_constptr_t SynthData::getProgram(int progID) const {
  return _bankdata->findProgram(progID);
}
prgdata_constptr_t SynthData::getProgramByName(const std::string& named) const {
  return _bankdata->findProgramByName(named);
}

///////////////////////////////////////////////////////////////////////////////

SynthData::SynthData()
    : _seqCursor(0.0f) {

  _bankdata = std::make_shared<BankData>();

  auto syn = synth::instance();
  _synsr   = syn->_sampleRate;

  // auto ObjDB = syn->_objectDB;
  // ObjDB->loadJson("krzfiles/krzdump.json");
  //_objects = ObjDB;

  // C4 = 72
  float t1, t2;

  ///////////////////////////////////////
  // timbreshift scan
  ///////////////////////////////////////

#if 0
	if( false ) for( int i=1; i<128; i++ )
	{
		auto prg = getKmTestProgram(1);
		auto l0 = prg->getLayer(0);

		for( int n=0; n<72; n+=1 )
		{
			for( int tst=0; tst<18; tst++ )
			{
				t1 = seqTime(.15f);

				addEvent( t1 ,[=]()
				{
					//l0->_km_timbreshift = (tst%9)*4;
					//l0->_km_transposeTS = (tst/9);
					auto pi = syn->keyOn(12+n,prg);

					addEvent( t1+1.5 ,[=]()
					{
						syn->keyOff(pi);
					} );
				});
			}
		}
	}

	///////////////////////////////////////
	// keymap scan
	///////////////////////////////////////

	if( false ) for( int i=1; i<128; i++ )
	{
		auto prg = getKmTestProgram(i);

		for( int n=0; n<48; n++ )
		{
			t1 = seqTime(0.15f);

			addEvent( t1 ,[=]()
			{
				auto pi = syn->keyOn(24+n,prg);

				addEvent( t1+2 ,[=]()
				{
					syn->keyOff(pi);
				} );
			});
		}
	}

	///////////////////////////////////////
	// program scan
	///////////////////////////////////////

	if( false ) for( int i=1; i<128; i++ )
	{
		for( int n=0; n<36; n+=1 )
		{
			t1 = seqTime(0.5f);
			//t2 = seqTime(0.1f);

			addEvent( t1 ,[=]()
			{
				auto prg = getProgram(i);
				auto pi = syn->keyOn(48+n,prg);

				addEvent( t1+0.5 ,[=]()
				{
					syn->keyOff(pi);
				} );
			});
		}
	}
#endif

  ///////////////////////////////////////

  //_lpf.setup(330.0f,syn->);
}

} // namespace ork::audio::singularity
