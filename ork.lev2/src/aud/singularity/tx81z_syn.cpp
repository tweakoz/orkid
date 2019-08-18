#include <math.h>
#include <assert.h>
#include "tx81z.h"
#include "synth.h"
#include "fmosc.h"
#include "alg_oscil.h"
#include <ork/kernel/string/string.h>

typedef std::function<void(DspBuffer& dspbuf)> fm4alg_t;

struct fm4vcpriv
{
    void callalg(DspBuffer& dspbuf)
    {
        _curalg(dspbuf);

        auto& HAF = _curlayer->_HAF;

        for( int i=0; i<4; i++ )
        {
            const auto& opd = _data._ops[i];
            auto& opf = HAF._op4frame[i];
            opf._mi = _mi[i];
            opf._r = _ratio[i];
            opf._f = _f[i];
            opf._envout = _opa[i];
            opf._olev = opd._outLevel;
            opf._ar = opd._atkRate;
            opf._d1r = opd._dec1Rate;
            opf._d1l = opd._dec1Lev;
            opf._d2r = opd._dec2Rate;
            opf._rr = opd._relRate;
            opf._egshift = opd._egShift;
            opf._wav = opd._waveform;
        }

        //printf( "f0<%f> f1<%f> f2<%f> f3<%f>\n", _f[0], _f[1], _f[2], _f[3] );
        //printf( "i0<%f> i1<%f> i2<%f> i3<%f>\n", _mi[0], _mi[1], _mi[2], _mi[3] );
        //printf( "a0<%f> a1<%f> a2<%f> a3<%f>\n", _opa[0], _opa[1], _opa[2], _opa[3] );
        //printf( "o0<%f> o1<%f> o2<%f> o3<%f>\n", opvalm(0), opvalm(1), opvalm(2), opvalm(3) );
    }
    fm4vcpriv()
   {
        /////////////////////////////////////////////////
        _alg[0] = [this](DspBuffer& dspbuf)
        {
            //   (3)->2->1->0
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],m2);
                float o0 = _fmosc[0].compute(_f[0],m1);

                U[i] = o0*_opa[0];
            }

        };
        /////////////////////////////////////////////////
        _alg[1] = [this](DspBuffer& dspbuf)
        {
            //  (3)\
            //   2->1->0
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],m2);
                float o0 = _fmosc[0].compute(_f[0],m1);

                U[i] = o0*_opa[0];
            }

        };
        /////////////////////////////////////////////////
        _alg[2] = [this](DspBuffer& dspbuf)
        {
            //   2
            //   1  (3)
            //     0
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],m2);
                float o0 = _fmosc[0].compute(_f[0],(m1+m3));

                U[i] = o0*_opa[0];
            }

        };
        /////////////////////////////////////////////////
        _alg[3] = [this](DspBuffer& dspbuf)
        {
            //    (3)
            //   1 2
            //    0
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],0.0f);
                float o0 = _fmosc[0].compute(_f[0],(m1+m2));

                U[i] = o0*_opa[0];
            }

        };
        /////////////////////////////////////////////////
        _alg[4] = [this](DspBuffer& dspbuf)
        {
            // 1 (3)
            // 0  2
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],0.0f);
                float o0 = _fmosc[0].compute(_f[0],m1);

                //printf( "_mi[1]<%f>\n", _mi[1] );
                U[i] = o0*_opa[0]*_olev[0]
                     + o2*_opa[2]*_olev[2];
            }

        };
        /////////////////////////////////////////////////
        _alg[5] = [this](DspBuffer& dspbuf)
        {
            //   (3)
            //   / \
            // 0  1  2
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],0.0f);
                float o0 = _fmosc[0].compute(_f[0],m3);

                U[i] = o0*_opa[0]+o1*_opa[1]+o2*_opa[2];
            }

        };
        /////////////////////////////////////////////////
        _alg[6] = [this](DspBuffer& dspbuf)
        {
            //      (3)
            // 0  1  2
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],m3);
                float o1 = _fmosc[1].compute(_f[1],0.0f);
                float o0 = _fmosc[0].compute(_f[0],0.0f);

                U[i] = o0*_opa[0]+o1*_opa[1]+o2*_opa[2];

            }

        };
        /////////////////////////////////////////////////
        _alg[7] = [this](DspBuffer& dspbuf)
        {
            //   0  1  2 (3)
            int inumframes = dspbuf._numframes;
            float* U = dspbuf.channel(0);

            for( int i=0; i<inumframes; i++ )
            {
                updateControllers();
                computeOpParms();
                float l3 = _fmosc[3]._prevOutput;
                float l2 = _fmosc[2]._prevOutput;
                float l1 = _fmosc[1]._prevOutput;
                float l0 = _fmosc[0]._prevOutput;
                float m3 = l3*_mi[3]*_opa[3];
                float m2 = l2*_mi[2]*_opa[2];
                float m1 = l1*_mi[1]*_opa[1];
                float m0 = l0*_mi[0]*_opa[0];

                float o3 = _fmosc[3].compute(_f[3],m3*FBL());
                float o2 = _fmosc[2].compute(_f[2],0.0f);
                float o1 = _fmosc[1].compute(_f[1],0.0f);
                float o0 = _fmosc[0].compute(_f[0],0.0f);

                U[i] = o0*_opa[0]+o1*_opa[1]+o2*_opa[2]+o3*_opa[3];
            }

        };
        /////////////////////////////////////////////////
   }
   //////////////////////////////////////////////////////////////
   float FBL()
   {
        float fbl = float(_data._feedback)/7.0f;
        return fbl;
   }
   //////////////////////////////////////////////////////////////
   void updateControllers()
   {
        float dt = 256/48000.0f;
   }
   //////////////////////////////////////////////////////////////
   void computeOpParms()
   {
        _mi[0] = computeModIndex(0);
        _mi[1] = computeModIndex(1);
        _mi[2] = computeModIndex(2);
        _mi[3] = computeModIndex(3);
        _f[0] = computeOpFrq(0);
        _f[1] = computeOpFrq(1);
        _f[2] = computeOpFrq(2);
        _f[3] = computeOpFrq(3);

   }
   //////////////////////////////////////////////////////////////
   float computeOpFrq(int op) const
   {
        const auto& opd = _data._ops[op];
        bool fixed = opd._fixedFrqMode;

        if( fixed )
        {
            return opd._frqFixed;
        }
        else
        {   float f = midi_note_to_frequency(_note)*opd._frqRatio;
            return f;
        }
   }
    //////////////////////////////////////////////////////////////
    float computeModIndex(int op) const
    {   const auto& opd = _data._ops[op];
        return opd._modIndex;
    }
    //////////////////////////////////////////////////////////////
    // op<3> ol<99> tl<0> mi<25.132742>
    // op<2> ol<70> tl<29> mi<2.037071>
    // op<1> ol<88> tl<11> mi<9.689997>
    // op<0> ol<73> tl<26> mi<2.641754>
    //////////////////////////////////////////////////////////////
    void keyOn(layer* l)
    {
        _curlayer = l;
        _newnote = true;
        for( int i=0; i<4; i++ )
        {
            const auto& opd = _data._ops[i];
            _mi[i] = 0.0f;
            _opa[i] = 0.0f;
            _ratio[i] = 0.0f;
            _fixed[i] = 0.0f;
            _f[i] = 0.0f;
            DspKeyOnInfo koi;
            _fmosc[i].keyOn(koi,opd);
            _olev[i] = float(opd._outLevel)/99.0f;
        }

        auto& HUDTEXT = l->_HKF._miscText;

        computeOpParms();

        if(0)for( int i=0; i<4; i++ )
        {
            const auto& opd = _data._ops[i];
            HUDTEXT += ork::FormatString("op<%d> olev<%d> wav<%d>\n", i, opd._outLevel, opd._waveform );
            HUDTEXT += ork::FormatString("       LS<%d> RS<%d>\n", i, opd._levScaling, opd._ratScaling );
        }
    }
    void keyOff()
    {
    }
    //////////////////////////////////////////////////////////////

    FmOsc    _fmosc[4];

    fm4alg_t _curalg;
    Fm4ProgData _data;
    fm4alg_t _alg[8];
    float _mi[4];
    float _f[4];
    float _xegval[4];
    float _opa[4];
    float _ratio[4];
    float _fixed[4];
    float _olev[4];
    bool _newnote = false;
    int _note;
    layer* _curlayer;
};

fm4syn::fm4syn()
{
    auto priv = new fm4vcpriv;
    _pimpl.Set<fm4vcpriv*>( priv );

}
void fm4syn::compute(DspBuffer& dspbuf)
{
    auto priv = _pimpl.Get<fm4vcpriv*>();
    for( int i=0; i<4; i++ )
    {
        priv->_opa[i] = _opAmp[i];
        //printf( "got amp<%d:%f>\n", i, _opAmp[i] );
    }
    priv->callalg(dspbuf);

}
void fm4syn::keyOn(const DspKeyOnInfo& koi)
{
    _opAmp[0] = 0.0f;
    _opAmp[1] = 0.0f;
    _opAmp[2] = 0.0f;
    _opAmp[3] = 0.0f;
    auto dspb = koi._prv;
    auto& dbd = dspb->_dbd;
    auto progd = dbd.getExtData("FM4").Get<Fm4ProgData*>();
    auto l = koi._layer;

   // int curpitch = l->_curnote;
    //printf( "_curnote<%d>\n", l->_curnote );
    _data = *progd;
    auto priv = _pimpl.Get<fm4vcpriv*>();

    priv->_note = l->_curnote + (_data._middleC-24);
    priv->_curalg = priv->_alg[_data._alg];
    priv->_data = *progd;

    l->_HKF._miscText = ork::FormatString("FM4 alg<%d> fbl<%d> MIDC<%d>\n", _data._alg, _data._feedback, _data._middleC );
    l->_HKF._useFm4 = true;

    priv->keyOn(l);

}
void fm4syn::keyOff()
{
    auto priv = _pimpl.Get<fm4vcpriv*>();
    priv->keyOff();
}
