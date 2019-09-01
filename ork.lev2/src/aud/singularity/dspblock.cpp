#include "synth.h"
#include <assert.h>
#include "filters.h"
#include "alg_eq.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

DspBuffer::DspBuffer()
    : _maxframes(0)
    , _numframes(0)
{
    for( int i=0; i<kmaxchans; i++ )
        _channels[i] = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void DspBuffer::resize(int inumframes)
{
    if( inumframes > _maxframes )
    {
        for( int i=0; i<kmaxchans; i++ )
        {
            if( _channels[i] )
                delete[] _channels[i];
            _channels[i] = new float[inumframes];
        }
        _maxframes = inumframes;
    }
    _numframes = inumframes;

}

float* DspBuffer::channel(int ich)
{
    return _channels[ich%kmaxchans];
}


///////////////////////////////////////////////////////////////////////////////

DspBlock::DspBlock(const DspBlockData& dbd)
    : _dbd(dbd)
    , _baseIndex(dbd._blockIndex)
    , _numParams(dbd._numParams)
{
}

///////////////////////////////////////////////////////////////////////////////

FPARAM DspBlock::initFPARAM(const DspParamData& dpd)
{
    FPARAM rval;
    rval._coarse = dpd._coarse;
    rval._fine = dpd._fine;
    rval._C1 = _layer->getSRC1(dpd._mods);
    rval._C2 = _layer->getSRC2(dpd._mods);
    rval._evaluator = dpd._mods._evaluator;

    rval._keyTrack = dpd._keyTrack;
    rval._velTrack = dpd._velTrack;
    rval._kstartNote = dpd._keystartNote;
    rval._kstartBipolar = dpd._keystartBipolar;

    return rval;

}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::keyOn(const DspKeyOnInfo& koi)
{
    _layer = koi._layer;

    for( int i=0; i<_numParams; i++ )
    {
        _param[i] = initFPARAM(_dbd._paramd[i]);
        _param[i].keyOn(koi._key,koi._vel);
    }

    doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getInpBuf1(DspBuffer& obuf)
{
    uint32_t imask = _iomask._inputMask;
    if( imask&1 ) return obuf.channel(0);
    if( imask&2 ) return obuf.channel(1);
    if( imask&4 ) return obuf.channel(2);
    if( imask&8 ) return obuf.channel(3);
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getOutBuf1(DspBuffer& obuf)
{
    uint32_t mask = _iomask._outputMask;
    if( mask&1 ) return obuf.channel(0);
    if( mask&2 ) return obuf.channel(1);
    if( mask&4 ) return obuf.channel(2);
    if( mask&8 ) return obuf.channel(3);
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::output1(DspBuffer& obuf,int index,float val)
{
    float* A = obuf.channel(0);
    float* B = obuf.channel(1);
    float* C = obuf.channel(2);
    float* D = obuf.channel(3);
    uint32_t omask = _iomask._outputMask;

    if( omask&1 )
        A[index] = val;
    if( omask&2 )
        B[index] = val;
    if( omask&4 )
        C[index] = val;
    if( omask&8 )
        D[index] = val;
}

///////////////////////////////////////////////////////////////////////////////

int NumberOfSetBits(uint32_t i)
{
     i = i - ((i >> 1) & 0x55555555);
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

int IoMask::numInputs() const
{
    return NumberOfSetBits(_inputMask);

}
int IoMask::numOutputs() const
{
    return NumberOfSetBits(_outputMask);
}

int DspBlock::numOutputs() const
{
    return _iomask.numOutputs();
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numInputs() const
{
    return _iomask.numInputs();
}

} // namespace ork::audio::singularity {
