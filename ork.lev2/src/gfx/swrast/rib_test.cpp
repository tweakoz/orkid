#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/cvector4.h>

#include "rib_test.h"
#include "render_graph.h"
#include <math.h>
#include <boost/foreach.hpp>
//#include <aqsis/ri/ri.h>

///////////////////////////////////////////////////////////////////////////////

struct RibOut
{
    std::vector<std::string>    mOutputStream;
    int                         miIndent;

    RibOut() : miIndent(0) {}

    RibOut& operator += ( const std::string& ins )
    {
        ork::FixedString<65536> rep;
        for( int i=0; i<miIndent; i++ ) rep += "\t";
        rep += ins.c_str();
        rep.replace_in_place("'","\"");
        mOutputStream.push_back(rep.c_str());
        
        return *this;
    }
    void AddLine( const std::string& ins )
    {
        
        this->operator+=(ins+"\n");
    }
    void PushIndent()
    {   miIndent++;
    }
    void PopIndent()
    {   miIndent--;
    }
    void Write( const std::string& path )
    {
        FILE* fout = fopen(path.c_str(),"wt");
        
        BOOST_FOREACH( const std::string& item, mOutputStream )
        {
            fprintf( fout, "%s", item.c_str() );
        }
        fclose(fout);
    }
};

///////////////////////////////////////////////////////////////////////////////

int rib_test_main(int argc, char** argv)
{
    printf( "Running RibTest\n" );

    RibOut ribo;
    
    ribo.AddLine("Display 'min.tiff' 'framebuffer' 'rgb'");
    ribo.AddLine("Format 1920 1080 1\n");
    ribo.AddLine("Option 'limits' 'bucketsize' [32 32] 'eyesplits' [16]");
    ribo.AddLine("PixelSamples 8 8");
    ribo.AddLine("ShadingRate 8");
    ribo.AddLine("Clipping 1 100");
    ribo.AddLine("DepthOfField 2.8 .2 5" );
    ribo.AddLine("Projection 'perspective'" );
    ribo.AddLine("WorldBegin");
    ribo.PushIndent();
    {
        ribo.AddLine("Declare 'from' 'point'" );
        ribo.AddLine("Declare 'to' 'point'" );
        ribo.AddLine("Declare 'lightcolor' 'color'" );

        ribo.AddLine("Displacement 'displacement/micro_bumps' 'float Kd' [0.4] 'float mult' [0.4]" );
        //RiSurface( "surface/metal", "float Ka", & bound, "float Ks", & bound, "float roughness", & kD, RI_NULL );
       // RiSurface( "surface/microscope", RI_NULL );
        //RiSurface( "surface/show_N", RI_NULL );
        //ribo.AddLine("Surface 'surface/randgrid'" );
        ribo.AddLine("Surface 'surface/show_N'" );
        ribo.AddLine("Attribute 'displacementbound' 'sphere' [1]");
        ribo.AddLine("Translate 0 0 15");
        ribo.AddLine("Sphere 10 -10 10 360");
    }
    ribo.PopIndent();
    ribo.AddLine("WorldEnd");
    
    ribo.Write("output.rib");
    return 0;
}
