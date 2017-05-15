#include <string>
#include <portaudio.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sstream>
#include <GLFW/glfw3.h>
#include "drawtext.h"

#include <FLAC++/decoder.h>

#include "krzdata.h"
#include "synth.h"
#include "krzobjects.h"

///////////////////////////////////////////////////////////////////////////////

static const int kdefaultprogID = 198;
//static const int kdefaultprogID = 21;
static int programID = 0;
static int octave = 4;
const programData* curProg = nullptr;

std::map<int,programInst*> playingNotesMap;
std::vector<SynthData*> _gBankSet;
int _gBankIndex = 0;

extern std::string kbasepath;// = "/usr/local/share/singularity";

///////////////////////////////////////////////////////////////////////////////

s16* getK2V3InternalSoundBlock()
{
    static s16* gdata = nullptr;
    if( nullptr == gdata )
    {
        std::string filename = kbasepath+"/kurzweil/k2v3internalsamplerom.bin";
        printf( "Loading Soundblock<%s>\n", filename.c_str() );
        FILE* fin = fopen(filename.c_str(), "rb");
        gdata = (s16*) malloc(8<<20);
        fread(gdata,8<<20,1,fin);
        fclose(fin);
      }
    return gdata;
}

///////////////////////////////////////////////////////////////////////////////

synth* the_synth = nullptr;

static int patestCallback(	const void *inputBuffer,
              							void *outputBuffer,
                           	unsigned long framesPerBuffer,
                           	const PaStreamCallbackTimeInfo* timeInfo,
                           	PaStreamCallbackFlags statusFlags,
                           	void *userData )
{
    /* Cast data passed through stream to our structure. */
    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */
    
    the_synth->compute(framesPerBuffer,inputBuffer);
    const auto& obuf = the_synth->_obuf;
    
    for( i=0; i<framesPerBuffer; i++ )
    {
        *out++ = obuf._leftBuffer[i];
        *out++ = obuf._rightBuffer[i];
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

PaStream* pa_stream = nullptr;

void startupAudio()
{
    the_synth = new synth(getSampleRate());
    auto err = Pa_Initialize();
    assert( err == paNoError );

    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &pa_stream,
                                1,          // no input channels 
                                2,          // stereo output 
                                paFloat32,  // 32 bit floating point output 
                                the_synth->_sampleRate,
                                256,        /* frames per buffer, i.e. the number
                                                   of sample frames that PortAudio will
                                                   request from the callback. Many apps
                                                   may want to use
                                                   paFramesPerBufferUnspecified, which
                                                   tells PortAudio to pick the best,
                                                   possibly changing, buffer size.*/
                                patestCallback, // this is your callback function 
                                nullptr ); //This is a pointer that will be passed to
                                         //         your callback
    
    assert(err==paNoError);

    err = Pa_StartStream( pa_stream );
    assert(err==paNoError);

}

///////////////////////////////////////////////////////////////////////////////

void tearDownAudio()
{
    auto err = Pa_StopStream( pa_stream );
    assert( err == paNoError );
    err = Pa_Terminate();
    assert( err == paNoError );
}

///////////////////////////////////////////////////////////////////////////////


bool loadprog(int pid)
{
  static bool gload = true;
  if(gload)
  {
    gload=false;
    //////////////////////////////
    // kurzweil
    //////////////////////////////
    if( 1 )
    {
      auto sd_KRZbase = new KrzSynthData(the_synth);
      /*KrzSynthData::baseObjects()->loadJson("analeads",200);
      KrzSynthData::baseObjects()->loadJson("anapads",300);
      KrzSynthData::baseObjects()->loadJson("anacomps",300);
      KrzSynthData::baseObjects()->loadJson("bass",300);
      KrzSynthData::baseObjects()->loadJson("bells",300);
      KrzSynthData::baseObjects()->loadJson("brass",300);
      KrzSynthData::baseObjects()->loadJson("hybperc",300);
      KrzSynthData::baseObjects()->loadJson("hybrids",300);
      KrzSynthData::baseObjects()->loadJson("strings",300);
      KrzSynthData::baseObjects()->loadJson("vox",300);*/
      _gBankSet.push_back(sd_KRZbase);
    }
    if( 0 )
    {
      auto sd_KRZtest = new KrzTestData(the_synth);
      auto sd_KRZkmtest = new KrzKmTestData(the_synth);
      _gBankSet.push_back(sd_KRZtest);
      _gBankSet.push_back(sd_KRZkmtest);
    }

    //////////////////////////////
    // czx (cz101)
    //////////////////////////////

    if( 0 )
    {
      auto sd_CZtest = new CzData(the_synth);
      //sd_CZtest->loadBank(kbasepath+"/cz101/patch_0.syx");
      //sd_CZtest->loadBank(kbasepath+"/cz101/AndreasStling.syx","humm");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/allnetcz/CASIO1.SYX","c1");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/allnetcz/CASIO2.SYX","c2");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/allnetcz/CASIO3.SYX","c3");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/allnetcz/CASIO4.SYX","c4");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/allnetcz/CASIO5.SYX","c5");
     _gBankSet.push_back(sd_CZtest);
      /*sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/czleads1/CZLEADS1.SYX","leads");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/cz1org64/CZ1ORGB1.SYX","orgb1");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/cz1org64/CZ1ORGB2.SYX","orgb2");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/cz1org64/CZ1ORGB3.SYX","orgb3");
      sd_CZtest->loadBank(kbasepath+"/cz101/cz5000/cz1org64/CZ1ORGB4.SYX","orgb4");
    */
    }  
    //////////////////////////////
    // fm4 (tx81z)
    //////////////////////////////

    if( 0 )
    {
      auto sd_FM4test = new Tx81zData(the_synth);

      sd_FM4test->loadBank(kbasepath+"/tx81z/presets_1.syx");
      sd_FM4test->loadBank(kbasepath+"/tx81z/presets_2.syx");
      sd_FM4test->loadBank(kbasepath+"/tx81z/presets_3.syx");
      sd_FM4test->loadBank(kbasepath+"/tx81z/presets_4.syx");
      //sd_FM4test->loadBank(kbasepath+"/tx81z/joeldump/Backup2016.05.24_01.syx");
      //sd_FM4test->loadBank(kbasepath+"/tx81z/joeldump/Backup2016.05.24_02.syx");
      //sd_FM4test->loadBank(kbasepath+"/tx81z/joeldump/Rip_May8th2015/Abank_TX81Z_Num1.syx");
      for( int i=1; i<41; i++ )
      {
          auto bankname = formatString("/tx81z/TX81Z%02dN.SYX", i);
          sd_FM4test->loadBank(kbasepath+bankname);
      }    
      _gBankSet.push_back(sd_FM4test);
    }

    auto loadsf2 = [](const std::string& name, const std::string& tag)
    {
      auto sbank = new Sf2TestSynthData(name, the_synth, tag );
      _gBankSet.push_back(sbank);
    };

    const bool load_all_sf2 = false;

    if( load_all_sf2 )
    {
      loadsf2("SteinwayModelLGrand.sf2","stein");
      loadsf2("Proteus 2000 Bank 0.sf2", "prot_2k0" );
      loadsf2("Proteus 2000 Bank 1.sf2", "prot_2k1" );
      loadsf2("Proteus 2000 Bank 2.sf2", "prot_2k2" );
      loadsf2("Proteus 2000 Bank 3.sf2", "prot_2k3" );
      loadsf2("Proteus 2000 Bank 4.sf2", "prot_2k4" );
      loadsf2("Proteus 2000 Bank 5.sf2", "prot_2k5" );
      loadsf2("Proteus 2000 Bank 6.sf2", "prot_2k6" );
    }
    if( load_all_sf2 )
    {
      loadsf2("Planet Earth Bank 0.sf2", "plearth0" );
      loadsf2("Planet Earth Bank 1.sf2", "plearth1" );
      loadsf2("Planet Earth Bank 2.sf2", "plarth2" );
      loadsf2("Planet Earth Bank 3.sf2", "plearth3" );
    }
    if( load_all_sf2 )
    {
      loadsf2("Orchestral  Winds, Brass, & Percussion.SF2", "virtuos0" );
      loadsf2("Orchestral Strings.SF2", "virtuos1" );
    }
    if( load_all_sf2 )
    {
      loadsf2("Xtreme Lead 1 Bank 0.SF2", "xtrlead0" );
      loadsf2("Xtreme Lead 1 Bank 1.SF2", "xtrlead1" );
      loadsf2("Xtreme Lead 1 Bank 2.SF2", "xtrlead2" );
      loadsf2("Xtreme Lead 1 Bank 3.SF2", "xtrlead3" );
    }
    if( load_all_sf2 )
    {
      loadsf2("Vintage Pro Bank 0.SF2", "vintpro0" );
      loadsf2("Vintage Pro Bank 1.SF2", "vintpro1" );
      loadsf2("Vintage Pro Bank 2.SF2", "vintpro2" );
      loadsf2("Vintage Pro Bank 3.SF2", "vintpro3" );
    }
    if( load_all_sf2 )
    {
      loadsf2("Mo' Phatt Bank 0.SF2", "mophatt0" );
      loadsf2("Mo' Phatt Bank 1.SF2", "mophatt1" );
      loadsf2("Mo' Phatt Bank 2.SF2", "mophatt2" );
      loadsf2("Mo' Phatt Bank 3.SF2", "mophatt3" );
    }
    if( load_all_sf2 )
    {
      loadsf2("DsfFunkKit.SF2", "dsfFunk" );
      loadsf2("DsfJazzKit.SF2", "dsfJazz" );
      loadsf2("DsfModernKit.SF2", "dsfModern" );
      loadsf2("DsfRecordingKit.SF2", "dsfRecording" );
      loadsf2("DsfRockKit.SF2", "dsfRock" );
      loadsf2("DsfStudioKit.SF2", "dsfStudio" );
    }


    /*
    auto sd_phatt = new Sf2TestSynthData("PhattInstruments.sf2",the_synth, "phatt");
    auto sd_prot2 = new Sf2TestSynthData("Proteus2 Instruments.sf2",the_synth, "proteus2");
    auto sd_prot3 = new Sf2TestSynthData("Proteus3 Instruments.sf2",the_synth, "proteus3");
    auto sd_vkeys = new Sf2TestSynthData("Vintage Instruments.sf2",the_synth, "vintkeys");
    */

    
    /*
    _gBankSet.push_back(sd_prot2);
    _gBankSet.push_back(sd_prot3);
    _gBankSet.push_back(sd_vkeys);
    _gBankSet.push_back(sd_phatt);
    */
  }
  //static auto sd_SF2test = new Sf2TestSynthData("Proteus3 Instruments.sf2",the_synth);
   //"grandpiano.sf2"
   //"Indperc.sf2"
   //"Vintage Instruments.sf2"
   //"Proteus2 Instruments.sf2"
   //"Proteus3 Instruments.sf2"
   //"sonatina/Concert Harp.sf2"
   //"sonatina/Keys - Grand Piano.sf2"
   //"sonatina/Chorus - Mixed.sf2"
   //"sonatina/Strings - Cello Solo.sf2"


  int bankid = _gBankIndex % _gBankSet.size();
  auto bank = _gBankSet[bankid];
  curProg = bank->getProgram(pid);

  if( curProg )
    the_synth->resetFenables();

  return bool(curProg);
};

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    bool down = ( action == GLFW_PRESS );
    bool up = ( action == GLFW_RELEASE );

    bool shift = (mods&1);
    //  return;

   // auto sd = the_synth->_SD;


    //printf( "key<%d>\n", key );

    auto do_midikb = [&]( int key, bool is_down )
    {
        int note = -1;

        switch(key)
        {
            case 'Z':
              note = 0;
              break;
            case 'S':
              note = 1;
              break;
            case 'X':
              note = 2;
              break;
            case 'D':
              note = 3;
              break;
            case 'C':
              note = 4;
              break;
            case 'V':        
              note = 5;
              break;
            case 'G':        
              note = 6;
              break;
            case 'B':        
              note = 7;
              break;
            case 'H':        
              note = 8;
              break;
            case 'N':        
              note = 9;
              break;
            case 'J':        
              note = 10;
              break;
            case 'M':        
              note = 11;
              break;
            case ',':        
              note = 12;
              break;
            case 'L':        
              note = 13;
              break;
            case '.':        
              note = 14;
              break;
            case ';':        
              note = 15;
              break;
            case '/':        
              note = 16;
              break;
            default:
              break;
        }

        if( note>=0 )
        {
            note = ((octave+1)*12) + note;

            if( is_down )
            {
                the_synth->addEvent( 0.0f ,[=]()
                {   
                    auto it = playingNotesMap.find(note);
                    if( it == playingNotesMap.end() )
                    {
                      auto pi = the_synth->keyOn(note,curProg);
                      assert(pi);
                      playingNotesMap[note] = pi;
                    }
                } );
            }
            else // must be up
            {
                the_synth->addEvent( 0.0f ,[=]()
                {
                    auto it = playingNotesMap.find(note);
                    if(it!=playingNotesMap.end())
                    {
                      auto pi = it->second;
                      assert(pi);
                      the_synth->keyOff(pi);
                      playingNotesMap.erase(it);
                    }
                } );
            }
        }

    };

    switch( key )
    {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case 262: // cursdown
          if( ! down ) break;
          programID = loadprog(programID+(shift?10:1))
                    ? programID+(shift?10:1)
                    : 1;
          break;
        case 263: // cursup
          if( ! down ) break;
          programID = loadprog(programID-(shift?10:1))
                    ? programID-(shift?10:1)
                    : 199;
          break;
        case 264: // cursleft
          {
            if( ! down ) break;
             programID = loadprog(programID-10)
                    ? programID-10
                    : 1;
          }
          break;
        case 265: // cursright
          if( ! down ) break;
          programID = loadprog(programID+10)
                    ? programID+10
                    : 1;
          break;
        case '-':
          if( ! down ) break;
          if( (octave-1) > 0 )
            octave--;
          break;
        case '=':
          if( ! down ) break;
          if( (octave+1) < 8 )
            octave++;
          break;
        case 344: // rshift 
          if( ! down ) break;
          _gBankIndex++;
          loadprog(programID);
          break;
        case 'Z':
        case 'S':
        case 'X':
        case 'D':
        case 'C':
        case 'V':
        case 'G':
        case 'B':
        case 'H':
        case 'N':
        case 'J':
        case 'M':
        case ',':
        case 'L':
        case '.':
        case ';':
        case '/':
          if( down || up )
            do_midikb( key, down );
          break;
        case 258: // tab
        {
          if( ! down ) break;

          int nl = curProg 
                  ? curProg->_layerDatas.size()
                  : 0;

          the_synth->_soloLayer++;

          if( the_synth->_soloLayer >= nl ) 
              the_synth->_soloLayer = -1;


          the_synth->resetFenables();

          printf( "inclayer: %d nl<%d>\n", the_synth->_soloLayer, nl);

          break;

        }
        case '`':
          if( ! down ) break;
          the_synth->_fblockEnable[0] = ! the_synth->_fblockEnable[0];
          break;
        case '1':
          if( ! down ) break;
          the_synth->_fblockEnable[1] = ! the_synth->_fblockEnable[1];
          break;
        case '2':
          if( ! down ) break;
          the_synth->_fblockEnable[2] = ! the_synth->_fblockEnable[2];
          break;
        case '3':
          if( ! down ) break;
          the_synth->_fblockEnable[3] = ! the_synth->_fblockEnable[3];
          break;
        case '4':
          if( ! down ) break;
          the_synth->_fblockEnable[4] = ! the_synth->_fblockEnable[4];
          break;
        case '5':
          if( ! down ) break;
          the_synth->_masterGain *= 0.707f;
          break;
        case '6':
          if( ! down ) break;
          the_synth->_masterGain *= 1.0f/0.707f;
          break;
        case '9':
          if( ! down ) break;
          the_synth->_bypassDSP = ! the_synth->_bypassDSP;
          break;
        case ' ':
          if( down or up ) 
            the_synth->_doModWheel = down; //! the_synth->_doModWheel;
          break;        
        case 257: // spc
          if( down or up ) 
            the_synth->_doPressure = down; //! the_synth->_doPressure;
          break;        
        case 340: // enter
          if( down  ) 
            the_synth->_doInput = ! the_synth->_doInput; //! the_synth->_doModWheel;
          break;        
        case 'Q':
          if( ! down ) break;
          the_synth->_hudpage = 0;
          break;
        case 'W':
          if( ! down ) break;
          the_synth->_hudpage = 1;
          break;
        case 'E':
          if( ! down ) break;
          the_synth->_hudpage = 2;
          break;
        case 'I':
          //if( ! down ) break;
          the_synth->_ostrack -= 1.f;
          break;
        case 'O':
          //if( ! down ) break;
          the_synth->_ostrack += 1.f;
          break;
        case 259: // delete
          if( ! down ) break;
          the_synth->_genmode = (the_synth->_genmode+1)%4;
          break;
/*        case 'P':
        {  auto l = the_synth->_hudLayer;
           bool inc = key=='P';
           if( up ) break;
           if( l && l->_kmregion )
           {  auto r = l->_kmregion;
              if( r && r->_sample )
              {
                auto s = (sample*) r->_sample;
                s->_pitchAdjust = inc 
                                ? s->_pitchAdjust+5
                                : s->_pitchAdjust-5;

              }
           }
           break;
        }*/
        default:
          break;
    }
}

///////////////////////////////////////////////////////////////////////////////

static int width = 0;
static int height = 0;

void drawtext( const std::string& str, float x, float y, float scale, float r, float g, float b )
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0,width,height,0,0,1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();    
    glTranslatef(x,y, 0);
    glScalef(scale,-scale,1);

    glColor4f(r,g,b,1);
    dtx_string(str.c_str());

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void runUI()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
      return;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1152, 760, "KrzTest", NULL, NULL);
    if (!window)
    {
      glfwTerminate();
      return;
    }

    glfwMakeContextCurrent(window);

    auto font = dtx_open_font("/Library/Fonts/Tahoma.ttf", 48);
    assert(font!=nullptr);
    dtx_use_font(font, 48);

    glfwSetKeyCallback(window, key_callback);

    //auto sd = the_synth->_SD;

    ///////////////////////////////////////////////////////

    while (!glfwWindowShouldClose(window))
    {
        glfwGetFramebufferSize(window, &width, &height);

        glClearColor(.20,0,.20,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0,0,width,height);
        glScissor(0,0,width,height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0,width,0,height,0,1);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        ///////////////////////////////

        bool modwh = the_synth->_doModWheel;
        bool press = the_synth->_doPressure;
        float mgain = the_synth->_masterGain;
        float mgainDB = linear_amp_ratio_to_decibel(mgain);

        auto hudstr = formatString("Octave<%d> Mod<%d> Press<%d> MGain<%ddB>\n", octave, int(modwh), int(press), int(mgainDB) );

        if( nullptr == curProg )
        {
          programID=kdefaultprogID;
          loadprog(programID);
        }

        const char* prghead = curProg->_role.c_str();

        hudstr += formatString("%s<%d:%s>\n", prghead,programID, curProg->_name.c_str() );
        drawtext( hudstr, 50,50, 1, 0,1,0 );

        ///////////////////////////////

        int inumlayers = curProg->_layerDatas.size();
        if( inumlayers > 3 )
            drawtext( "(Drum Program)\n", 80,150, .65, 0,1,0 );
        else for( int i=0; i<inumlayers; i++ )
        {
          auto ld = curProg->_layerDatas[i];
          auto km = ld->_keymap;
          if( km )
          {
              hudstr = formatString(" L%d keymap<%d:%s>\n", i+1, km->_kmID, km->_name.c_str() );
              bool sololayer = the_synth->_soloLayer==i;
              float r = sololayer ? 1 : 0;

              drawtext( hudstr, 80,150+(i*29), .65, r,1,0 );
          }
          else
          {

          }
        }


        ///////////////////////////////

        glLoadIdentity();    
        glColor4f(1,1,0,1);
        the_synth->onDrawHud(width,height);

        ///////////////////////////////

        glFinish();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}

///////////////////////////////////////////////////////////////////////////////

int SingularityMain(int argc, const char** argv)
{
    getK2V3InternalSoundBlock();
    usleep(1<<19);



  	startupAudio();

    runUI();

  	tearDownAudio();

    printf( "Goodbye...\n");
  	return 0;
}

///////////////////////////////////////////////////////////////////////////////

std::string formatString( const char* formatstring, ... )
{
    std::string rval;
    char formatbuffer[512];

    va_list args;
    va_start(args, formatstring);
    //buffer.vformat(formatstring, args);
    vsnprintf( &formatbuffer[0], sizeof(formatbuffer), formatstring, args );
    va_end(args);
    rval = formatbuffer;
    return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SplitString(const std::string& s, char delim, std::vector<std::string>& tokens)
{ std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim))
        tokens.push_back(item);
}

std::vector<std::string> SplitString(const std::string& instr, char delim)
{ std::vector<std::string> tokens;
    SplitString(instr, delim, tokens);
    return tokens;
}
