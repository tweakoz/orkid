#include "hud.h"
#if 0

///////////////////////////////////////////////////////////////////////////////

void DrawEnv(const ItemDrawReq& EDR)
{
    const auto& R = EDR.rect;

    float X2 = R.X1+R.W;
    float Y2 = R.Y1+R.H;

    const auto& KFIN = EDR.s->_curhud_kframe;
    const auto& AFIN = EDR.s->_curhud_aframe;
    const auto& ENVFRAME = EDR._data.Get<envframe>();
    const RateLevelEnvData* ENVD = ENVFRAME._data;

    const auto& ENVCT = EDR.ld->_envCtrlData;
    bool collsamp = EDR.shouldCollectSample();

    bool useNENV = ENVCT._useNatEnv&&(EDR.ienv==0);

    std::string sampname = "???";

    if( useNENV )
        sampname = "natural";
    else
        sampname = formatString("env%d", EDR.ienv+1);


    assert(EDR.ienv<kmaxenvperlayer);

    auto lyr = EDR.l;

    /////////////////////////////////////////////////
    // collect hud samples
    /////////////////////////////////////////////////

    auto& HUDSAMPS = EDR.s->_hudsample_map[sampname];

    if( collsamp )
    {
        hudsample hs;
        hs._time = EDR.s->_lnotetime;
        hs._value = ENVFRAME._value;
        HUDSAMPS.push_back(hs);
    }

    /////////////////////////////////////////////////
    // draw envelope values
    /////////////////////////////////////////////////

    float env_by = R.Y1+20;
    float env_bx = R.X1+70.0;
    int spcperseg = 70;
    if( useNENV )
    {
        const auto sample = KFIN._kmregion->_sample;
        const auto& NES = sample->_natenv;
        int nseg = NES.size();
        int icurseg = ENVFRAME._curseg;

        drawtext( "NATENV", env_bx,env_by, fontscale, 1,0,0 );

        for( int i=0; i<nseg; i++ )
        {
            bool iscurseg = (i==icurseg);
            float r = 1;
            float g = iscurseg ? 0.5 : 1;

            auto hudstr = formatString("seg%d",i);
            drawtext( hudstr, env_bx+spcperseg*i,env_by+20, fontscale, r,g,0 );
        }
        drawtext( "dB/s\ntim", R.X1+15,env_by+40, .45, 1,1,0 );
        for( int i=0; i<nseg; i++ )
        {
            const auto& seg = NES[i];

            auto hudstr = formatString("%0.1f\n%d",seg._slope, int(seg._time));
            bool iscurseg = (i==icurseg);
            float r = 1;
            float g = iscurseg ? 0 : 1;
            float b = iscurseg ? 0 : 1;
            drawtext( hudstr, env_bx+spcperseg*i,env_by+40, fontscale, r,g,1 );
        }
    }
    else if( ENVD )
    {

        auto& AE = ENVD->_segments;
        drawtext( ENVD->_name, R.X1,env_by, fontscale, 1,0,.5 );
        int icurseg = ENVFRAME._curseg;
        for( int i=0; i<7; i++ )
        {
            std::string segname;
            if(i<3)
                segname = formatString("atk%d",i+1);
            else if(i==3)
                segname = "dec";
            else
                segname = formatString("rel%d",i-3);
            auto hudstr = formatString("%s",segname.c_str());

            bool iscurseg = (i==icurseg);
            float r = 1;
            float g = iscurseg ? 0.5 : 1;

            drawtext( hudstr, env_bx+spcperseg*i,env_by+20, fontscale, r,g,0 );
        }
        drawtext( "lev\ntim", R.X1+15,env_by+40, .45, 1,1,0 );
        for( int i=0; i<7; i++ )
        {
            auto hudstr = formatString("%0.2f\n%0.2f",AE[i]._level, AE[i]._rate);
            bool iscurseg = (i==icurseg);
            float r = 1;
            float g = iscurseg ? 0 : 1;
            float b = iscurseg ? 0 : 1;
            drawtext( hudstr, env_bx+spcperseg*i,env_by+40, fontscale, r,g,1 );
        }
    }

    if( false == (EDR.ienv==0 or ENVD) )
        return;

    /////////////////////////////////////////////////
    // draw envelope segments
    /////////////////////////////////////////////////

    float fxb = R.X1;
    float fyb = Y2;
    float fw = R.W;
    float fpx = fxb;
    float fpy = fyb;

    R.PushOrtho();

    ///////////////////////
    // draw border
    ///////////////////////

    DrawBorder(R.X1,R.Y1,X2,Y2);

    const float ktime = 20.0f;

    float fh = useNENV
               ? 200
               : 400;

    bool bipolar = ENVD ? ENVD->_bipolar : false;

    ///////////////////////
    // draw grid, origin
    ///////////////////////

    glColor4f(.5,.2,.5,1);
    glBegin(GL_LINES);
    if( bipolar )
    {
        float fy = fyb-(fh*0.5f)*0.5f;
        float x1 = fxb;
        float x2 = x1+fw;

        glVertex3f(x1,fy,0);
        glVertex3f(x2,fy,0);
    }
    glEnd();

    ///////////////////////
    // from hud samples
    ///////////////////////

    glColor4f(1,1,1,1);
    glBegin(GL_LINES);
    for( int i=0; i<HUDSAMPS.size(); i++ )
    {   const auto& hs = HUDSAMPS[i];
        if( fpx >= R.X1 and fpx <= X2 )
        {
            glVertex3f(fpx,fpy,0);
            fpx = fxb + hs._time*(fw/ktime);

            float fval = hs._value;
            if( bipolar )
                fval = 0.5f+(fval*0.5f);
            fpy = fyb - (fh*0.5f)*fval;

            glVertex3f(fpx,fpy,0);
        }
    }
    glEnd();

    ///////////////////////
    // natural env dB slopehull
    ///////////////////////

    if( useNENV )
    {
        glColor4f(1,0,0,1);
        fpx = fxb;
        fpy = fyb;
        glBegin(GL_LINES);
        for( int i=0; i<HUDSAMPS.size(); i++ )
        {   const auto& hs = HUDSAMPS[i];
            glVertex3f(fpx,fpy,0);

            float val = hs._value;
            val = 96.0 + linear_amp_ratio_to_decibel(val);
            if( val<0.0f ) val = 0.0f;
            val = val/96.0f;

            fpx = fxb + hs._time*(fw/ktime);
            fpy = fyb - fh*val;
            glVertex3f(fpx,fpy,0);
        }
        glEnd();
    }
    R.PopOrtho();

    /////////////////////////////////////////////////
}
#endif
