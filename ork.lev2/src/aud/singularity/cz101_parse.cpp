#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include "krzdata.h"
#include "krzobjects.h"
#include "cz101.h"

using namespace ork;

void parse_cz101(CzData* outd, const std::string& path,const std::string& bnkname)
{
    ork::CFile syxfile(path.c_str(),ork::EFM_READ);
    u8* data = nullptr;
    size_t size = 0;
    syxfile.Load((void**)(&data),size);

    printf( "cz101 syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));
    
    auto zpmDB = outd->_zpmDB;
    int programcount = 0;
    int prgbase = 0;

    switch( size )
    {
        case 296:
            assert(data[0]==0xf0 and data[1]==0x44);
            programcount = 1;
            prgbase = 8;
            break;
        case 4224: // 16
            assert(data[0]==0xf0 and data[1]==0x44);
            programcount = 16;
            prgbase = 7;
            break;
        default:
            assert(false);
            break;
    }

    for( int iv=0; iv<programcount; iv++ )
    {
        printf( "////////////////////////////\n");
        auto prg = new programData;
        zpmDB->_programs[outd->_lastprg++] = prg;
        prg->_role = "czx";

        std::vector<u8> dbytes;
        for( int i=0; i<128; i++ )
        {
            int bidx = prgbase+(i*2)+(256+8)*iv;
            U8 ln = data[bidx+0];
            U8 hn = data[bidx+1];
            U8 byte = (hn<<4)|ln;
            dbytes.push_back(byte);

        }
        if(0)
        for( int r=0; r<16; r++ )
        {
            int bidx = (r*8);
            printf( "%03d: ", bidx);

            for( int c=0; c<8; c++ )
            {
                U8 byte = dbytes[bidx+c];
                printf( "%02x ", byte);
            }

            printf( "\n");
        }

        auto czdata = new CzProgData;
        u8 PFLAG = dbytes[0x00]; // octave/linesel
        czdata->_octave = (PFLAG&0x0c)>>2;
        czdata->_lineSel = (PFLAG&0x03);

        u8 PDS = dbytes[0x01]; // detune sign
        u8 PDETL = dbytes[0x02]; // detune fine
        u8 PDETH = dbytes[0x03]; // detune oct/note
        int detval = (PDETH*100)+((PDETL*100)/63);
        czdata->_detuneCents = PDS ? (-detval) : detval;

        u8 PVK = dbytes[0x04]; // vibrato wave
        switch( PVK )
        {
            case 0x08:
                czdata->_vibratoWave = 1;
                break;
            case 0x04:
                czdata->_vibratoWave = 2;
                break;
            case 0x20:
                czdata->_vibratoWave = 3;
                break;
            case 0x02:
                czdata->_vibratoWave = 4;
                break;
            default:
                assert(false);
                break;
        }
        u8 PVDLD = dbytes[0x05]; // vibrato delay (skip 2)
        czdata->_vibratoDelay = PVDLD;
        u8 PVSD = dbytes[0x08]; // vibrato rate (skip 2)
        czdata->_vibratoRate = PVSD;
        u8 PVDD = dbytes[0x0b]; // vibrato depth (skip 2)
        czdata->_vibratoDepth = PVDD;

        int byteindex = 0x0e; // OSC START

        for( int o=0; o<2; o++ )
        {        
            auto& OSC = czdata->_oscData[o];


            u8 MFW0 = dbytes[byteindex++]; // dc01 wave / modulat
            u8 MFW1 = dbytes[byteindex++]; // dc01 wave / modulat
            auto genwave = [](int c0, int c1) -> int
            {
                switch(c0)
                {
                    case 0:
                    case 1:
                    case 2:
                        return c0;
                        break;
                    case 4:
                        return 3;
                        break;
                    case 5:
                        return 4;
                        break;
                    case 6:
                        switch(c1)
                        {
                            case 1:
                                return 5;
                                break;
                            case 2:
                                return 6;
                                break;
                            case 3:
                                return 7;
                                break;
                        }
                        break;
                    default:
                        assert(false);
                }
                return -1;
            };

            OSC._dcoWaveA = genwave( (MFW0>>5)&0x7, (MFW1>>6)&0x3);
            OSC._dcoWaveB = genwave( (MFW0>>2)&0x7, (MFW1>>6)&0x3);
            if( o == 0 ) // ignore linemod from line1
            {
                czdata->_lineMod = (MFW1>>3)&0x7;
            }
            u8 MAMD = dbytes[byteindex+=2]; // DCA1 key follow (skip1)
            OSC._dcaKeyFollow = MAMD;
            u8 MWMD = dbytes[byteindex+=2]; // DCW1 key follow (skip1)
            OSC._dcwKeyFollow = MWMD;
            u8 PMAL = dbytes[byteindex++]; // DCA1 end
            OSC._dcaEnv._endStep = PMAL;
            // 0x15..0x24
            for( int i=0; i<8; i++ )
            {   u8 r = dbytes[byteindex++]; // byte = (119*r/99)
                u8 l = dbytes[byteindex++]; // byte = (127*l/99)
                u8 r7 = r&0x7f;
                u8 l7 = l&0x7f;
                OSC._dcaEnv._rate[i] = (r7*99)/119;
                OSC._dcaEnv._level[i] = (l7*99)/127;
            }
            // 16 bytes - DCA1 8x rate, level
            u8 PMWL = dbytes[byteindex++]; // DCW1 end
            OSC._dcwEnv._endStep = PMWL;
            // 0x26..0x35
            for( int i=0; i<8; i++ )
            {   u8 r = dbytes[byteindex++]; // byte = (119*r/99)+8
                u8 l = dbytes[byteindex++]; // byte = (127*l/99)
                u8 r7 = r&0x7f;
                u8 l7 = l&0x7f;
                if( r&0x80 )
                    OSC._dcwEnv._sustPoint = i;
                OSC._dcwEnv._rate[i] = (((r7-8)*99)/119);
                OSC._dcwEnv._level[i] = (l7*99)/127;
            }
            // 16 bytes - DCW1 8x rate, level
            u8 PMPL = dbytes[byteindex++]; // DCO1 end
            OSC._dcoEnv._endStep = PMPL;
            // 0x37..0x46
            for( int i=0; i<8; i++ )
            {   u8 r = dbytes[byteindex++]; // byte = (127*r/99)
                u8 l = dbytes[byteindex++]; // byte = (127*l/99)
                u8 r7 = r&0x7f;
                u8 l7 = l&0x7f;
                OSC._dcoEnv._rate[i] = (r7*99)/127;
                OSC._dcoEnv._level[i] = (l7*99)/127;
            }
            // 16 bytes - DCO1 8x rate, level
        }
        assert(byteindex==128);
//        char namebuf[11];
//        for( int n=0; n<10; n++ )
//            namebuf[n] = data[index_vb+57+n];

       // namebuf[10] = 0;

        prg->_name = FormatString("%s(%02d)",bnkname.c_str(),iv);

        //czdata->dump();
        auto ld = prg->newLayer();
        ld->_keymap = outd->_zpmKM;

        auto CB0 = new controlBlockData;
        ld->_ctrlBlocks[0] = CB0;

        auto AE = new RateLevelEnvData;
        CB0->_cdata[0] = AE;

        AE->_name = "AMPENV";
        AE->_ampenv = true;
        AE->_segments.push_back({0,1}); // atk1
        AE->_segments.push_back({0,0}); // atk2
        AE->_segments.push_back({0,0}); // atk3
        AE->_segments.push_back({0,0}); // dec
        AE->_segments.push_back({2,0}); // rel1
        AE->_segments.push_back({0,0}); // rel2
        AE->_segments.push_back({0,0}); // rel3
        ld->_kmpBlock._keymap = outd->_zpmKM;
        
        /*auto& F0 = ld->_dspBlocks[0];
        auto& F1 = ld->_dspBlocks[1];
        auto& F4 = ld->_dspBlocks[4];
        F0._dspBlock = "SAMPLEPB";
        //ld->_fBlock[0]._paramScheme = "PCH";
        F1._dspBlock = "CZX";
        //ld->_fBlock[1]._paramScheme = "PCH";
        F1._extdata["PDX"].Set<CzProgData*>(czdata);
        F4._dspBlock = "AMP";
        //ld->_fBlock[4]._paramScheme = "AMP";
        ld->_envCtrlData._useNatEnv = false;
        ld->_algData._algID = 1;
        ld->_algData._name = "ALG1";*/
    }

}

void CzProgData::dump() const
{
    printf( "CZPROG\n");
    printf( "  _octave<%d>\n", _octave );
    printf( "  _lineSel<%d>\n", _lineSel );
    printf( "  _lineMod<%d>\n", _lineMod );
    printf( "  _detuneCents<%d>\n", _detuneCents );
    printf( "  _vibratoWave<%d>\n", _vibratoWave );
    printf( "  _vibratoRate<%d>\n", _vibratoRate );
    printf( "  _vibratoDepth<%d>\n", _vibratoDepth );
    for( int o=0; o<2; o++ )
    {
        const auto& OSC = _oscData[o];
        assert(OSC._dcoWaveA>=0);
        assert(OSC._dcoWaveB>=0);
        assert(OSC._dcoWaveA<8);
        assert(OSC._dcoWaveB<8);
        printf( "  osc<%d>\n", o );
        printf( "    _dcoWaveA<%d>\n", OSC._dcoWaveA );
        printf( "    _dcoWaveB<%d>\n", OSC._dcoWaveB );
        printf( "    _dcaKeyFollow<%d>\n", OSC._dcaKeyFollow );
        printf( "    _dcwKeyFollow<%d>\n", OSC._dcwKeyFollow );
        auto dumpenv = [](const CzEnvelope& env)
        {
            printf( "        _endStep<%d>\n", env._endStep );
            if( env._sustPoint >= 0 )
                printf( "        _sustPoint<%d>\n", env._sustPoint );
            printf( "        r: ");
            for( int i=0; i<8; i++ )
                printf( "%02d ", env._rate[i] );
            printf( "\n");
            printf( "        l: ");
            for( int i=0; i<8; i++ )
                printf( "%02d ", env._level[i] );
            printf( "\n");
        };
        printf( "    _dcoEnv\n" );
        dumpenv(OSC._dcoEnv);
        printf( "    _dcwEnv\n" );
        dumpenv(OSC._dcwEnv);
        printf( "    _dcaEnv\n" );
        dumpenv(OSC._dcaEnv);

    }
}


CzData::CzData(synth* syn)
    : SynthData(syn)
    , _lastprg(0)
{
    _zpmDB = new VastObjectsDB;
    _zpmKM = new keymap;
    _zpmKM->_name = "CZX";
    _zpmKM->_kmID = 1;
    _zpmDB->_keymaps[1] = _zpmKM;
}

CzData::~CzData()
{

}
void CzData::loadBank(const std::string& syxpath,const std::string& bnkname)
{
    parse_cz101(this,syxpath,bnkname);
}
const programData* CzData::getProgram(int progID) const // final
{
    auto ObjDB = this->_zpmDB;
    return ObjDB->findProgram(progID);
}
