//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "krzdata.h"
#include "synth.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// Natural envelopes
///////////////////////////////////////////////////////////////////////////////

NatEnv::NatEnv()
    : _curseg(0)
    , _numseg(0)
    , _prvseg(0)
    , _slopePerSecond(0.0f)
    , _slopePerSample(0.0f)
    , _SR(getSampleRate())
    , _framesrem(0)
    , _state(0)
    , _layer(nullptr)
{

}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::keyOn(const KeyOnInfo& KOI,const sample* s)
{
    auto ld = KOI._layerData;
    assert(ld!=nullptr);
    _layer = KOI._layer;
    _layer->retain();

    int ikey = KOI._key;

    _natenvseg.clear();
    for( const auto& item : s->_natenv )
        _natenvseg.push_back( item );
    _numseg = _natenvseg.size();
    _curseg = 0;
    _prvseg = -1;
    _curamp = 1.0f;
    _state = 1;
    _ignoreRelease = ld->_ignRels;

    const auto& EC = ld->_envCtrlData;
    const auto& DKT = EC._decKeyTrack;
    const auto& RKT = EC._relKeyTrack;

    _decAdjust = EC._decAdjust;
    _relAdjust = EC._relAdjust;

    if( ikey>60 )
    {
        float flerp = float(ikey-60)/float(127-60);
        printf( "flerp<%f>\n", flerp );
        _decAdjust = lerp(_decAdjust,DKT,flerp);
        _relAdjust = lerp(_relAdjust,RKT,flerp);
    }
    else if( ikey<60 )
    {
        float flerp = float(59-ikey)/59.0f;
        _decAdjust = lerp(_decAdjust,1.0/DKT,flerp);
        _relAdjust = lerp(_relAdjust,1.0/RKT,flerp);
    }

    printf( "NATENV::keyOn ikey<%d> RA<%f> DA<%f> DKT<%f> RKT<%f>\n", ikey, _relAdjust, _decAdjust, DKT, RKT );

    initSeg(0);
}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::keyOff()
{
    _state=2;
    if( _ignoreRelease )
        return;

    if( _numseg-1 >= 0 )
        initSeg(_numseg-1);
}

///////////////////////////////////////////////////////////////////////////////

const natenvseg& NatEnv::getCurSeg() const
{
    assert(_curseg>=0);
    assert(_curseg<_numseg);
    return _natenvseg[_curseg];
}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::initSeg(int iseg)
{
    _curseg = iseg;
    const auto& seg = getCurSeg();

    _slopePerSecond = seg._slope;

    switch(_state)
    {
        case 2:
            _slopePerSecond *=_relAdjust;
            break;
        default:
            _slopePerSecond *=_decAdjust;
            break;
    }

    _slopePerSample = slopeDBPerSample(_slopePerSecond,192000.0);
    _segtime = seg._time;
    _framesrem = seg._time; ///16.0f;// * _SR / 48000.0f;
    printf( "SEG<%d/%d> CURAMP<%f> SLOPEPERSEC<%f> "
            "_slopePerSample<%f>  SEGT<%f> RA<%f> DA<%f>\n",
            _curseg+1, _numseg, _curamp,_slopePerSecond,
            _slopePerSample,seg._time, _relAdjust, _decAdjust );
}

///////////////////////////////////////////////////////////////////////////////

float NatEnv::compute()
{
    bool doslope = true;

    //printf( "a: _curamp<%f> _slopePerSample<%f>\n", _curamp, _slopePerSample );
    switch(_state)
    {
        case 1:
        {
            //doslope = true;
            _framesrem--;
            if( _framesrem<=0 )
            {
                if( _curseg+2 < _numseg )
                    initSeg(_curseg+1);
                else //
                {
                    //doslope = false;
                    //printf( "decay.. dbps<%f>\n",_slopePerSecond);
                }
            }
            break;
        }
        case 2: // released
        {
            float dbatten = linear_amp_ratio_to_decibel(_curamp);
            if( ((_curseg+1)==_numseg) and (dbatten<-72.0f) )
            {
                _state = 3;
                _layer->release();
            }
            break;
        }
        case 3: // dead
            doslope = false;
            break;
        default:
            break;
    }
    if( doslope )
        _curamp *= _slopePerSample;

    _curamp = softsat(_curamp,1.01f);
//    assert(_curamp<=1.0f);
    _curamp = clip_float(_curamp,0,1);
    //assert(_curamp>=-1.0f);
    //if( ((_curseg+1)==_numseg) and (dbatten<-96.0f) )
    //if( _layer && _layer->isHudLayer() )
     //   printf( "seg<%d> _curamp<%f> _slopePerSecond<%f> _slopePerSample<%f>\n", _curseg, _curamp, _slopePerSecond, _slopePerSample );
    //assert(false);
    return _curamp;
}

} // namespace ork::audio::singularity {
