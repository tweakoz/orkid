////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace reflect { 
///////////////////////////////////////////////////////////////////////////////

template<typename FunctionType> struct Function {};

///////////////////////////////////////////////////////////////////////////////

template<>
struct Function<void (*)()>
{
    typedef void ReturnType;
    typedef void (*FunctionType)();

    struct Parameters__
    {
        enum { Count = 0 };
        void Apply(BidirectionalSerializer &bidi, int) { bidi.Fail(); };
        void Apply(BidirectionalSerializer &bidi, int) const { bidi.Fail(); }
    };

    typedef Parameters__ Parameters;

    static void invoke(FunctionType f, const Parameters &)
    {
        return (*f)();
    }

    static void SetParameters(void *) {}
};

///////////////////////////////////////////////////////////////////////////////

template<typename R>
struct Function<R (*)()>
{
    typedef Function<void (*)()> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)();

    struct Parameters__
    {
        enum { Count = 0 };
        void Apply(BidirectionalSerializer &bidi, int) { bidi.Fail(); };
        void Apply(BidirectionalSerializer &bidi, int) const { bidi.Fail(); }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &)
    {
        return (*f)();
    }
};

///////////////////////////////////////////////////////////////////////////////
template<typename R, typename P0>
struct Function<R (*)(P0)>
{
    typedef Function<R (*)()> ParentFunction__;
    typedef Function<void (*)(P0)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P0 mParam_00;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, &mParam_00, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0>
inline void SetParameters(ReturnType (*)(P0), void *param_data, P0 a0)
{
    typedef typename Function<ReturnType (*)(P0)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename P0, typename P1>
struct Function<R (*)(P0, P1)>
{
    typedef Function<R (*)(P0)> ParentFunction__;
    typedef Function<void (*)(P0, P1)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P1 mParam_01;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, &mParam_01, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1>
inline void SetParameters(ReturnType (*)(P0, P1), void *param_data, P0 a0, P1 a1)
{
    typedef typename Function<ReturnType (*)(P0, P1)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename P0, typename P1, typename P2>
struct Function<R (*)(P0, P1, P2)>
{
    typedef Function<R (*)(P0, P1)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P2 mParam_02;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, &mParam_02, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2>
inline void SetParameters(ReturnType (*)(P0, P1, P2), void *param_data, P0 a0, P1 a1, P2 a2)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename P0, typename P1, typename P2, typename P3>
struct Function<R (*)(P0, P1, P2, P3)>
{
    typedef Function<R (*)(P0, P1, P2)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P3 mParam_03;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, &mParam_03, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
}

///////////////////////////////////////////////////////////////////////////////

template<	typename R, typename P0, typename P1,
			typename P2, typename P3, typename P4>
struct Function<R (*)(P0, P1, P2, P3, P4)>
{
    typedef Function<R (*)(P0, P1, P2, P3)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P4 mParam_04;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, &mParam_04, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5>
struct Function<R (*)(P0, P1, P2, P3, P4, P5)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P5 mParam_05;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, &mParam_05, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P6 mParam_06;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, &mParam_06, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P7 mParam_07;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, &mParam_07, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P8 mParam_08;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, &mParam_08, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P9 mParam_09;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, &mParam_09, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P10 mParam_10;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, &mParam_10, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P11 mParam_11;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, &mParam_11, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P12 mParam_12;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, &mParam_12, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P13 mParam_13;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, &mParam_13, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P14 mParam_14;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, &mParam_14, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14, typename P15>
struct Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)>
{
    typedef Function<R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)> GenericFunction__;
    typedef R ReturnType;
    typedef R (*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P15 mParam_15;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, &mParam_15, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(FunctionType f, const Parameters &params)
    {
        return (*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14, params.mParam_15);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14, typename P15>
inline void SetParameters(ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14, P15 a15)
{
    typedef typename Function<ReturnType (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
    params->mParam_15 = a15;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C>
struct Function<R (C::*)()>
{
    typedef Function<void (*)()> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)();

    struct Parameters__
    {
        enum { Count = 0 };
        void Apply(BidirectionalSerializer &bidi, int) { bidi.Fail(); };
        void Apply(BidirectionalSerializer &bidi, int) const { bidi.Fail(); }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &)
    {
        return (object.*f)();
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0>
struct Function<R (C::*)(P0)>
{
    typedef Function<R (C::*)()> ParentFunction__;
    typedef Function<void (*)(P0)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P0 mParam_00;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, &mParam_00, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0>
inline void SetParameters(ReturnType (ClassType::*)(P0), void *param_data, P0 a0)
{
    typedef typename Function<ReturnType (ClassType::*)(P0)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1>
struct Function<R (C::*)(P0, P1)>
{
    typedef Function<R (C::*)(P0)> ParentFunction__;
    typedef Function<void (*)(P0, P1)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P1 mParam_01;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, &mParam_01, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1), void *param_data, P0 a0, P1 a1)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2>
struct Function<R (C::*)(P0, P1, P2)>
{
    typedef Function<R (C::*)(P0, P1)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P2 mParam_02;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, &mParam_02, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2), void *param_data, P0 a0, P1 a1, P2 a2)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3>
struct Function<R (C::*)(P0, P1, P2, P3)>
{
    typedef Function<R (C::*)(P0, P1, P2)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P3 mParam_03;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, &mParam_03, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4>
struct Function<R (C::*)(P0, P1, P2, P3, P4)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P4 mParam_04;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, &mParam_04, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P5 mParam_05;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, &mParam_05, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P6 mParam_06;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, &mParam_06, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P7 mParam_07;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, &mParam_07, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P8 mParam_08;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, &mParam_08, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P9 mParam_09;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, &mParam_09, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P10 mParam_10;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, &mParam_10, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P11 mParam_11;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, &mParam_11, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P12 mParam_12;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, &mParam_12, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P13 mParam_13;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, &mParam_13, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P14 mParam_14;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, &mParam_14, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14, typename P15>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P15 mParam_15;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, &mParam_15, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14, params.mParam_15);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14, typename P15>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15), void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14, P15 a15)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
    params->mParam_15 = a15;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C>
struct Function<R (C::*)() const>
{
    typedef Function<void (*)()> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)() const;

    struct Parameters__
    {
        enum { Count = 0 };
        void Apply(BidirectionalSerializer &bidi, int) { bidi.Fail(); };
        void Apply(BidirectionalSerializer &bidi, int) const { bidi.Fail(); }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &)
    {
        return (object.*f)();
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0>
struct Function<R (C::*)(P0) const>
{
    typedef Function<R (C::*)()> ParentFunction__;
    typedef Function<void (*)(P0)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P0 mParam_00;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, &mParam_00, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P0>(&mParam_00, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0>
inline void SetParameters(ReturnType (ClassType::*)(P0) const, void *param_data, P0 a0)
{
    typedef typename Function<ReturnType (ClassType::*)(P0) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1>
struct Function<R (C::*)(P0, P1) const>
{
    typedef Function<R (C::*)(P0)> ParentFunction__;
    typedef Function<void (*)(P0, P1)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P1 mParam_01;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, &mParam_01, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P1>(&mParam_01, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1) const, void *param_data, P0 a0, P1 a1)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2>
struct Function<R (C::*)(P0, P1, P2) const>
{
    typedef Function<R (C::*)(P0, P1)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P2 mParam_02;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, &mParam_02, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P2>(&mParam_02, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2) const, void *param_data, P0 a0, P1 a1, P2 a2)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3>
struct Function<R (C::*)(P0, P1, P2, P3) const>
{
    typedef Function<R (C::*)(P0, P1, P2)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P3 mParam_03;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, &mParam_03, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P3>(&mParam_03, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4>
struct Function<R (C::*)(P0, P1, P2, P3, P4) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P4 mParam_04;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, &mParam_04, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P4>(&mParam_04, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P5 mParam_05;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, &mParam_05, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P5>(&mParam_05, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P6 mParam_06;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, &mParam_06, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P6>(&mParam_06, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P7 mParam_07;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, &mParam_07, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P7>(&mParam_07, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P8 mParam_08;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, &mParam_08, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P8>(&mParam_08, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P9 mParam_09;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, &mParam_09, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P9>(&mParam_09, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P10 mParam_10;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, &mParam_10, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P10>(&mParam_10, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P11 mParam_11;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, &mParam_11, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P11>(&mParam_11, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P12 mParam_12;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, &mParam_12, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P12>(&mParam_12, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P13 mParam_13;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, &mParam_13, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P13>(&mParam_13, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P14 mParam_14;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, &mParam_14, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P14>(&mParam_14, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
}

///////////////////////////////////////////////////////////////////////////////

template<typename R, typename C,
    typename P0, typename P1, typename P2, typename P3,
    typename P4, typename P5, typename P6, typename P7,
    typename P8, typename P9, typename P10, typename P11,
    typename P12, typename P13, typename P14, typename P15>
struct Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15) const>
{
    typedef Function<R (C::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14)> ParentFunction__;
    typedef Function<void (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15)> GenericFunction__;
    typedef R ReturnType;
    typedef C ClassType;
    typedef R (C::*FunctionType)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15) const;

    struct Parameters__ : public ParentFunction__::Parameters
    {
        P15 mParam_15;

        enum { Count = 1 + ParentFunction__::Parameters::Count };

        void Apply(BidirectionalSerializer &bidi, int parameter_index)
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, &mParam_15, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }

        void Apply(BidirectionalSerializer &bidi, int parameter_index) const
        {
            if(parameter_index == Count - 1) Serialize<P15>(&mParam_15, NULL, bidi);
            else ParentFunction__::Parameters::Apply(bidi, parameter_index);
        }
    };

    typedef typename GenericFunction__::Parameters__ Parameters;

    static R invoke(ClassType &object, FunctionType f, const Parameters &params)
    {
        return (object.*f)(params.mParam_00, params.mParam_01, params.mParam_02, params.mParam_03, 
            params.mParam_04, params.mParam_05, params.mParam_06, params.mParam_07, 
            params.mParam_08, params.mParam_09, params.mParam_10, params.mParam_11, 
            params.mParam_12, params.mParam_13, params.mParam_14, params.mParam_15);
    }
};

///////////////////////////////////////////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12, typename P13, typename P14, typename P15>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15) const, void *param_data, P0 a0, P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8, P9 a9, P10 a10, P11 a11, P12 a12, P13 a13, P14 a14, P15 a15)
{
    typedef typename Function<ReturnType (ClassType::*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15) const>::Parameters Parameters;
    Parameters *params = static_cast<Parameters *>(param_data);
    params->mParam_00 = a0;
    params->mParam_01 = a1;
    params->mParam_02 = a2;
    params->mParam_03 = a3;
    params->mParam_04 = a4;
    params->mParam_05 = a5;
    params->mParam_06 = a6;
    params->mParam_07 = a7;
    params->mParam_08 = a8;
    params->mParam_09 = a9;
    params->mParam_10 = a10;
    params->mParam_11 = a11;
    params->mParam_12 = a12;
    params->mParam_13 = a13;
    params->mParam_14 = a14;
    params->mParam_15 = a15;
}

} }
