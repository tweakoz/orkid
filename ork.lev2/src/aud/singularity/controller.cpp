//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "krzdata.h"
#include "synth.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void ControlBlockInst::keyOn(const KeyOnInfo& KOI, const controlBlockData* CBD)
{
    assert(CBD);
    auto l = KOI._layer;
    auto& syn = l->_syn;

    for( int i=0; i<kmaxctrlperblock; i++ )
    {
        auto data = CBD->_cdata[i];
        if( data )
        {
            _cinst[i] = data->instantiate(syn);
            l->_controlMap[data->_name] = _cinst[i];
            _cinst[i]->keyOn(KOI);
        }
    }
}
void ControlBlockInst::keyOff()
{
    for( int i=0; i<kmaxctrlperblock; i++ )
    {
        if( _cinst[i] )
            _cinst[i]->keyOff();
    }
}
void ControlBlockInst::compute(int inumfr)
{
    for( int i=0; i<kmaxctrlperblock; i++ )
    {
        if( _cinst[i] )
            _cinst[i]->compute(inumfr);
    }
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst::ControllerInst(synth& syn)
    : _syn(syn)
    , _curval(0.0f)
{

}

} // namespace ork::audio::singularity {
