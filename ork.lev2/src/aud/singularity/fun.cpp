#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "synth.h"

///////////////////////////////////////////////////////////////////////////////

ControllerInst* FunData::instantiate(synth& syn) const // final
{
    return new FunInst(syn,this);
}

///////////////////////////////////////////////////////////////////////////////

FunInst::FunInst(synth& syn, const FunData* data)
    : ControllerInst(syn)
    , _data(data)
{
    _a = []()->float{return 0.0f;};
    _b = []()->float{return 0.0f;};
    _op = []()->float{return 0.0f;};
}

///////////////////////////////////////////////////////////////////////////////

void FunInst::compute(int inumfr) // final
{
    _curval = _op();
}

///////////////////////////////////////////////////////////////////////////////

struct Lowpass
{
    Lowpass() 
        :a(0.99f)
        ,b(1.f - a)
        ,z(0)
    {}
    float compute(float in)
    {   z = (in * b) + (z * a);
        return z;
    }

    float a, b, z;
};

void FunInst::keyOn(const KeyOnInfo& KOI) // final
{
    auto l = KOI._layer;

    if(nullptr==_data)
        return;

    _a = l->getController(_data->_a);   
    _b = l->getController(_data->_b);   
    _op = []()->float{return 0.0f;};

    const auto& op = _data->_op;
    if( 0 )
        return;
    else if( op=="a+b" )
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a+b);
        };  
    else if( op=="a-b" )
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a-b);
        };  
    else if( op=="a*b" )
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a*b);
        };  
    else if( op=="(a+b)/2" )
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a+b)*0.5f;
        };  
    else if( op=="a/2+b" )
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a*0.5f)+b;
        };  
    else if( op=="Quantize B To A" )
        _op = [=]()->float{
            float a = fabs(this->_a());
            int inumsteps = int(a*15.0f)+1;

            float b = this->_b();

            if( a == 1 )
                return clip_float(b,-1,1);

            int ib = b*float(inumsteps);

            float bb = float(ib)/float(inumsteps);

            return clip_float(bb,-1,1);///inva;
        };  
    else if( op=="lowpass(f=a,b)" )
    {
        auto lowpass = new Lowpass;
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            float out = lowpass->compute(b);
            return out;
        };
    }
    else if( op=="Sample B on A" )
    {
        auto state1 = new float(0.0f);
        auto state2 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            if(a>0.5f and (*state2)<0.5f )
                (*state1) = b;
            (*state2) = a;
            return (*state1);
        };
    }
    else if( op=="Track B While A" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            if(a>0.5f)
                (*state1) = b;
            return (*state1);
        };
    }
    else if( op=="a OR b" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            bool a = (this->_a()>0.5f);
            bool b = (this->_b()>0.5f);
            return float(a or b);
        };
    }
    else if( op=="max(a,b)" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a>b) ? a : b;
        };
    }
    else if( op=="min(a,b)" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a<b) ? a : b;
        };
    }
    else if( op=="(a+2b)/3" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return (a+2.0f*b)*0.3333333f;
        };
    }
    else if( op=="|a-b|" )
    {
        auto state1 = new float(0.0f);
        _op = [=]()->float{
            float a = this->_a();
            float b = this->_b();
            return fabs(a-b);
        };
    }
}
void FunInst::keyOff() // final
{
}
