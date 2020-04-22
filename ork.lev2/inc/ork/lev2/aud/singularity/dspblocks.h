#pragma once

#include "modulation.h"
#include "filters.h"
#include "para.h"
#include "PolyBLEP.h"
#include "sampleOsc.h"
#include "layer.h"
#include <ork/kernel/svariant.h>
#include "alg.h"

namespace ork::audio::singularity {

struct outputBuffer;
struct DspBlock;

///////////////////////////////////////////////////////////////////////////////

struct DspBuffer
{
    DspBuffer();
    void resize(int inumframes);

    float* channel(int ich);

    int _maxframes;
    int _numframes;
private:
    static const int kmaxchans = 4;
    float* _channels[kmaxchans];

};

///////////////////////////////////////////////////////////////////////////////

struct DspBlock
{
    DspBlock(const DspBlockData& dbd);
    virtual ~DspBlock() {}

    void keyOn(const DspKeyOnInfo& koi);
    void keyOff(layer*l);

    virtual void compute(DspBuffer& dspbuf) = 0;

    virtual void doKeyOn(const DspKeyOnInfo& koi) {}
    virtual void doKeyOff() {}

    float* getInpBuf1(DspBuffer& dspbuf);
    float* getOutBuf1(DspBuffer& dspbuf);

    FPARAM initFPARAM(const DspParamData& dpd);

    void output1(DspBuffer& dspbuf,int index,float val);

    const DspBlockData _dbd;
    int _baseIndex = -1;
    int _numParams;
    int numOutputs() const;
    int numInputs() const;
    layer* _layer = nullptr;

    float _fval[kmaxparmperblock];
    FPARAM _param[kmaxparmperblock];

    IoMask _iomask;
};

} //namespace ork::audio::singularity {
