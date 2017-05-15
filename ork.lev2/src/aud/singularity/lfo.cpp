#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "synth.h"

///////////////////////////////////////////////////////////////////////////////

LfoData::LfoData()
    : _initialPhase(0.0f)
    , _minRate(1.0f)
    , _maxRate(1.0f)
    , _shape("Sine")
{

}

ControllerInst* LfoData::instantiate(synth& syn) const 
{
    auto r = new LfoInst( syn, this );
    return r;
}

///////////////////////////////////////////////////////////////////////////////

LfoInst::LfoInst(synth& syn, const LfoData* data)
    : ControllerInst(syn)
    , _data(data)
    , _phaseInc(0.0f)
    , _phase(0.0f)
    , _currate(0.0f)
    , _enabled(false)
    , _rateLerp(0.0f)
    , _bias(0.0f)
{
    _mapper = [](float inp)->float{return 0.0f;};
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::reset()
{
    _phaseInc=(0.0f);
    _phase=(0.0f);
    _currate=(0.0f);
    _enabled = false;
    _rateLerp=0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::keyOn(const KeyOnInfo& KOI) // final
{
    if( nullptr == _data )
        return;

    _phase = _data->_initialPhase*0.25f;
    _enabled = true;
    _rateLerp = 0.0f;
    _bias = 0.0f;
    if(_data->_controller=="ON")
    {
        _rateLerp = 1.0f;
    }
    if( _data->_shape == "Sine" )
        _mapper = [](float inp)->float{return sinf(inp*pi2);};
    else if( _data->_shape == "None" )
        _mapper = [](float inp)->float{return 0.0f;};
    else if( _data->_shape == "+Sine" )
        _mapper = [](float inp)->float{return 0.5f+sinf(inp*pi2)*0.5f;};
    else if( _data->_shape == "Triangle" )
        _mapper = [](float inp)->float{
            float tri = fabs(fmod(inp*4.0f,4.0f)-2.0f)-1.0f;
            return tri;
    };
    else if( _data->_shape == "+Triangle" )
        _mapper = [](float inp)->float{
            float tri = fabs(fmod(inp*4.0f,4.0f)-2.0f)*0.5f;
            return tri;
    };
    else if( _data->_shape == "Square" )
        _mapper = [](float inp)->float{
            float squ = sinf(inp*pi2) >= 0.0f 
                      ? +1.0f
                      : -1.0f;
            return squ;
    };
    else if( _data->_shape == "+Square" )
        _mapper = [](float inp)->float{
            float squ = sinf(inp*pi2) >= 0.0f 
                      ? +1.0f
                      : 0.0f;
            return squ;
    };
    else if( _data->_shape == "Rise Saw" )
        _mapper = [](float inp)->float{
            float saw = fmod(inp*2.0f,2.0f)-1.0f;
            return saw;
    };
    else if( _data->_shape == "Fall Saw" )
        _mapper = [](float inp)->float{
            float saw = fmod(inp*2.0f,2.0f)-1.0f;
            return -saw;
    };
    else if( _data->_shape == "4 Step" )
        _mapper = [](float inp)->float{
            static const float table[] = {
                0.25f,
                1.0f,
                -1.0f,
                -.25,
            };
            int x = int(inp*4.0f)&3;
            return table[x];

        };
    else if( _data->_shape == "+4 Step" )
        _mapper = [](float inp)->float{
            static const float table[] = {
                0.666666f, //cents_to_linear_freq_ratio(900),
                1.0f,
                0.0f,
                0.333333f //cents_to_linear_freq_ratio(300), 
            };  
            int x = int(inp*4.0f)&3;
            return table[x];

        };
    else if( _data->_shape == "+3 Step" )
        _mapper = [](float inp)->float{
            static const float table[] = {
                1.0f,
                0.0f,
                0.5f,
            };
            int x = int((inp+0.3333f)*3.0f)%3;
            return table[x];

        };
    else
        _mapper = [](float inp)->float{return float(rand()%100)*0.01f;};
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::keyOff() // final
{
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::compute(int inumfr) // final
{
    if(nullptr==_data)
    {   
        _curval = 0.0f;
    }
    else
    {
       float dt = float(inumfr)/_syn._sampleRate;
      
       _currate = lerp(_data->_minRate,_data->_maxRate,_rateLerp);
        
        //printf( "lforate<%f>\n", rate );
        _phaseInc = dt*_currate;
        _phase += _phaseInc;
        _curval = _mapper(_phase);
    }
}

