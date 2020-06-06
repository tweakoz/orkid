#include "krzio.h"
#include <fstream>

using namespace rapidjson;

///////////////////////////////////////////////////////////////////////////////

template <typename T>
struct keymap_ent_templ
{
	typedef void(^kmsetter_t)(Keymap*k,RegionData&rd, T tval);
	
	static void doit(Keymap* kmap, kmsetter_t krt, int ivelrng, const std::vector<T>& the_vec, const char* typnam, T invval)
	{
		int inumV = the_vec.size();
		int iprevV = invval;
		int ivcnt = 0;
		int is = 0;
		int ilo, ihi;

		for( int i=0; i<inumV; i++ )
		{	
			T val = the_vec[i];

			RegionData& rdata = kmap->RegionDataForKeyVel(i,ivelrng);
		
			krt(kmap,rdata,val);

			int inexV = (i<(inumV-1)) ? the_vec[i+1] : invval;
			if( val!=iprevV )
			{				
				if( (is%3)==0 )
				{	
					if(is!=0)
					{
						//gxmlout += CreateFormattedString("\n");
					}
					//gxmlout += CreateFormattedString("			");
				}
				//gxmlout += CreateFormattedString("<%s note='%x' ", typnam, i );
				ilo = i;
				std::stringstream ss;
				ss << "v='" << val << "'";
				//gxmlout += ss.str();
				ivcnt=1;
				is++;
			}
			if( val!=inexV )
			{
				//gxmlout += CreateFormattedString(" c='%x'/> ", ivcnt);
				ihi=ilo+(ivcnt-1);
			}
			ivcnt++;
			iprevV=val;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseKeyMapEntryIt(Keymap* kmap, const datablock& db, datablock::iterator& it, int ivelrng )
{
	const char* velrngnams[8] = 
	{
		"ppp",
		"pp",
		"p",
		"mp",
		"mf",
		"f",
		"ff",
		"fff",
	};

	const char* velrangenam = velrngnams[ivelrng];

	int inument = kmap->miKrzNumEntries;
	int method = kmap->miKrzMethod;
	int ientsize = kmap->miKrzEntrySize;

	u16 offset;
	bool bOK = db.GetData( offset, it );
	//printf( "offset<%d>\n", offset );
	datablock::iterator ret( it, offset-2 );
	u8 u8v;
	u16 u16v;
	//printf( "      [%s]\n", velrange );
	bool bsubsample = (method&1);
	bool bsample = (method&2);
	bool bvoladj = (method&4);
	bool b1btune = (method&8);
	bool b2btune = (method&16);
	
	bool bindent = true;

	//gxmlout += CreateFormattedString("	<range r='%s' inument='%d' et_2btune='%d' et_1btune='%d' et_voladj='%d' et_samp='%d' et_subsamp='%d'>\n", velrangenam, inument, int(b2btune), int(b1btune), int(bvoladj), int(bsample), int(bsubsample) );

	bool newsubrange = true;
	bool endsubrange = false;

	std::vector<int> vect_subsamp;
	std::vector<int> vect_samp;
	std::vector<int> vect_2btune;
	std::vector<int> vect_1btune;
	std::vector<float> vect_voladj;
	for( int ie=0; ie<=inument; ie++ )
	{

		/////////////////////////////////
		u8 usubsample;
		u16 usample;
		s16 s2btune;
		s8 s1btune, svoladj;
		//printf( "<" );
		if( b2btune ) // &16
		{
			bOK = db.GetData( s2btune, ret );
			vect_2btune.push_back(int(s2btune));		
			//printf( "%dcts ", int(s2btune) );
			//gxmlout += CreateFormattedString("<tune2b cts=%d/>", int(s2btune) );
		}
		if( b1btune ) // &8
		{
			bOK = db.GetData( s1btune, ret );			
			vect_1btune.push_back(int(s1btune));		
			//printf( "%dcts ", int(s1btune) );
			//gxmlout += CreateFormattedString("<tune1b cts=%d/>", int(s1btune) );
		}
		if( bvoladj ) // &4
		{
			bOK = db.GetData( svoladj, ret );
			float fDB = float(svoladj)*0.5f;		
			vect_voladj.push_back(fDB);		
			//printf( "%3.1fdB ", fDB );
			//gxmlout += CreateFormattedString("<voladj dB=%3.1f/>", fDB );
		}
		if( bsample ) // &2
		{
			bOK = db.GetData( usample, ret );	
			vect_samp.push_back(int(usample));		
			//printf( "s:%d ", int(usample) );
			//gxmlout += CreateFormattedString("<sample id=%d/>", int(usample) );
		}
		if( bsubsample )
		{
			bOK = db.GetData( usubsample, ret );
			vect_subsamp.push_back(int(usubsample));		
			//printf( "ss:%d", int(usubsample) );
			//gxmlout += CreateFormattedString("<subsample idx=%d/>", int(usubsample) );
		}
		//printf( ">");
		/////////////////////////////////

	}

	//////////////////////
	// fill in 2d array of region data
	//////////////////////

	auto set_sample = ^void(Keymap*k,RegionData&rd,int ival) { rd.miSampleId = ival; };
	auto set_subsample = ^void(Keymap*k,RegionData&rd,int ival) { rd.miSubSample = ival-1; };
	auto set_2btune = ^void(Keymap*k,RegionData&rd,int ival) { rd.miTuning = ival; };
	auto set_1btune = ^void(Keymap*k,RegionData&rd,int ival) { rd.miTuning = ival; };
	auto set_voladj = ^void(Keymap*k,RegionData&rd,float fval) { rd.mVolumeAdjust = fval; };

	keymap_ent_templ<int>::doit(kmap,set_sample,ivelrng,vect_samp,"sample",-1);			
	keymap_ent_templ<int>::doit(kmap,set_subsample,ivelrng,vect_subsamp,"subsamp",-1);	
	keymap_ent_templ<int>::doit(kmap,set_2btune,ivelrng,vect_2btune,"tune2b",-1);		
	keymap_ent_templ<int>::doit(kmap,set_1btune,ivelrng,vect_1btune,"tune1b",-1);		
	keymap_ent_templ<float>::doit(kmap,set_voladj,ivelrng,vect_voladj,"voladj",-666.0f);

	//////////////////////

	//it = ret;
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseKeyMap(	const datablock& db,
								datablock::iterator& it,
								int iObjectID,
								std::string ObjName
							)
{

	Keymap* kmap = new Keymap;
	kmap->miKeymapID = iObjectID;
	kmap->mKeymapName = ObjName;

	_keymaps[iObjectID] = kmap;

	u16 uSampleHeaderID, uMethod, uBasePitch, uCentsPerEntry, uNumberOfEntries, uEntrySize;

	bool bOK = db.GetData( uSampleHeaderID, it );
	bOK = db.GetData( uMethod, it );
	bOK = db.GetData( uBasePitch, it );
	bOK = db.GetData( uCentsPerEntry, it );
	bOK = db.GetData( uNumberOfEntries, it );
	bOK = db.GetData( uEntrySize, it );

	kmap->miKeymapSampleId = int(uSampleHeaderID);
	kmap->miKeymapBasePitch = int(uBasePitch);
	kmap->miKeymapCentsPerEntry = int(uCentsPerEntry);
	kmap->miKrzMethod = int(uMethod);
	kmap->miKrzNumEntries = int(uNumberOfEntries);
	kmap->miKrzEntrySize = int(uEntrySize);

	//printf( "KEYMAP[%03d:%s]  SampleHeaderID<%d> NumEntriesPerVelRange<%d> EntrySize<%d> Method<0x%04x>\n", iObjectID, ObjName.c_str(), uSampleHeaderID, uNumberOfEntries, uEntrySize, uMethod );
	//printf( "  BasePitch<%d> CentsPerEntry<%d>\n", uBasePitch, uCentsPerEntry );

	//gxmlout += CreateFormattedString("<keymap id='%d' name='%s' sampid='%d' meth='%x' basep='%x' >\n", iObjectID, ObjName.c_str(), int(uSampleHeaderID), int(uMethod), int(uBasePitch) );

	ParseKeyMapEntryIt(kmap,db,it,0);	// ppp
	ParseKeyMapEntryIt(kmap,db,it,1);	// pp
	ParseKeyMapEntryIt(kmap,db,it,2);	// p
	ParseKeyMapEntryIt(kmap,db,it,3);	// mp
	ParseKeyMapEntryIt(kmap,db,it,4);	// mf
	ParseKeyMapEntryIt(kmap,db,it,5);	// f
	ParseKeyMapEntryIt(kmap,db,it,6);	// ff
	ParseKeyMapEntryIt(kmap,db,it,7);	// fff

	//gxmlout += CreateFormattedString("</keymap>\n" );

///////////////////////////////////////////////////////////////////////////////

	//////////////////////
	// create unique region set
	//////////////////////

	std::set<const RegionData*> ursets;
	for( int ivel=0; ivel<8; ivel++ )
	{
		for( int ikey=0; ikey<128; ikey++ )
		{
			const RegionData& rd_kv = kmap->RegionDataForKeyVel(ikey,ivel);
			kmap->mRegionSet.insert(rd_kv);
		}
	}
	for( const auto& item : kmap->mRegionSet )
	{
		const RegionData* uniqued = & item;
		ursets.insert(uniqued);
	}
	for( int ivel=0; ivel<8; ivel++ )
	{
		for( int ikey=0; ikey<128; ikey++ )
		{
			const RegionData& rd_kv = kmap->RegionDataForKeyVel(ikey,ivel);
			Keymap::regionset_t::const_iterator it=kmap->mRegionSet.find(rd_kv);
			assert(it!=kmap->mRegionSet.end());
			const RegionData* uniqued = & (*it);
			kmap->mRgnPtrMap[kmap->RegionMapIndex(ikey,ivel)]=uniqued;
		}
	}
	
	//////////////////////
	// create rectangular regions
	//////////////////////
	
	int ikmsampid = int(uSampleHeaderID);

	while( ursets.empty() == false )
	{
		auto rdata_it = ursets.begin();
		const RegionData* refrd = *rdata_it;

		bool prevmatch = false;
		for( int ivel=0; ivel<8; ivel++ )
		{
			for( int ikey=0; ikey<128; ikey++ )
			{
				int idx = kmap->RegionMapIndex(ikey,ivel);
				const RegionData* testrd = kmap->mRgnPtrMap[idx];

				bool bmatch = (refrd==testrd);

				if( bmatch ) // found region
				{
					int ilok = ikey;
					int ihik = ikey;
					int ilov = ivel;
					int ihiv = ivel;
					////////////////////////////////////
					// get horizontal extent
					////////////////////////////////////
					for( int ka=ilok; ka<=127; ka++ )
					{
						int jdx = kmap->RegionMapIndex(ka,ivel);
						const RegionData* jestrd = kmap->mRgnPtrMap[jdx];
						if( jestrd == refrd )
							ihik = ka;
						else 
							break;
					}
					////////////////////////////////////
					// get vertical extent
					////////////////////////////////////
					for( int jvel=ivel+1; jvel<8; jvel++ )
					{
						bool bvok = true;
						for( int ka=ilok; ka<=ihik; ka++ )
						{
							int jdx = kmap->RegionMapIndex(ka,jvel);
							const RegionData* jestrd = kmap->mRgnPtrMap[jdx];
							if( jestrd != refrd )
							{
								bvok=false;
							}
						}
						if( bvok ) 
							ihiv = jvel;
						else
							break;
					}
					////////////////////////////////////
					// add region
					////////////////////////////////////

					int iactualsampid = (refrd->miSampleId==0) 
					                  ? ikmsampid 
					                  : refrd->miSampleId;

					if( iactualsampid )
					{
						RegionInst ri;
						ri.miLoKey = ilok;
						ri.miHiKey = ihik;
						ri.miLoVel = ilov;
						ri.miHiVel = ihiv;
						ri.mData = *refrd;
						ri.mData.miSampleId = iactualsampid;

						int sid = ri.mData.miSampleId;
						int ssidx = ri.mData.miSubSample;

						int idlsk = GenSampleKey(sid,ssidx);
						//printf( "addrgni s<%d> ss<%d> idlsk<%08x>\n",sid,ssidx, idlsk );
						kmap->mRegionInsts.insert(ri);
					}

					////////////////////////////////////
					// clear region so its not counted twice
					////////////////////////////////////
					for( int ka=ilok; ka<=ihik; ka++ )
					{
						for( int va=ilov; va<=ihiv; va++ )
						{
							int sdx = kmap->RegionMapIndex(ka,va);
							kmap->mRgnPtrMap[sdx] = 0;
						}

					}
				}
			}
		}

		ursets.erase(rdata_it);
	}
	
	//////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitKeymap(const Keymap* km, rapidjson::Value& parent)
{
	if( km->miKeymapID > 200 )
		return;

	rapidjson::Value kmapobject(kObjectType); 
	AddStringKVMember(kmapobject,"Keymap", km->mKeymapName);
	kmapobject.AddMember("objectID", km->miKeymapID, _japrog);

	rapidjson::Value regionarrayobject(kArrayType); 
	for( const auto& r : km->mRegionInsts )
	{
		auto sid = r.mData.miSampleId;
		auto ssid = r.mData.miSubSample;

		rapidjson::Value rgnobject(kObjectType); 
		rgnobject.AddMember("loKey", r.miLoKey, _japrog);
		rgnobject.AddMember("hiKey", r.miHiKey, _japrog);
		rgnobject.AddMember("loVel", r.miLoVel, _japrog);
		rgnobject.AddMember("hiVel", r.miHiVel, _japrog);
		rgnobject.AddMember("tuning", r.mData.miTuning, _japrog);
		rgnobject.AddMember("volAdj", r.mData.mVolumeAdjust, _japrog);

		rgnobject.AddMember("multiSampleID", sid, _japrog);
		rgnobject.AddMember("subSampleID", ssid, _japrog);

		auto itms = _samples.find(sid);
		if( itms!=_samples.end() )
		{
			auto ms = itms->second;
			auto sname = ms->_multiSampleName;
			if( ssid < ms->_subSamples.size() )
			{
				auto s = ms->_subSamples[ssid];
				sname = s->_subSampleName;
			}
			AddStringKVMember(rgnobject,"sampleName", sname);
		}
		else
		{
			AddStringKVMember(rgnobject,"sampleName", "????");
		}

		regionarrayobject.PushBack(rgnobject, _japrog);

	}
	kmapobject.AddMember("regions", regionarrayobject, _japrog);

	parent.PushBack(kmapobject, _japrog);
}

///////////////////////////////////////////////////////////////////////////////
