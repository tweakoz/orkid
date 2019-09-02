////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "skin_filter.h"
#include <entity/AnimSeqEvent.h>
#include <file/chunkfile.h>
#include <file/file.h>
#include <file/tinyxml/tinyxml.h>
#include <miniork_tool_pch.h>
//#include <
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
/*
///////////////////////////////////////////////////////////////////////////////

class EntSkinAssetFilterInterface : public ork::lev2::CAssetFilterInterface
{
public:
    virtual bool ConvertAsset( const std::string & inf, const std::string & outf );
    EntSkinAssetFilterInterface();
};

///////////////////////////////////////////////////////////////////////////////

EntitySkinFilter::EntitySkinFilter( const CClass *pClass )
    : CObject( pClass )
{
}

void EntitySkinFilter::ManualClassInit( CClass *pClass )
{
    struct iface
    {
        static IInterface::Context *GetInterface( void )
        {
            static EntSkinAssetFilterInterface iface;
            return & iface;
        }
    };

    pClass->AddNamedInterface( "Convert", reinterpret_cast<IInterface::Callback>( iface::GetInterface ) );
    pClass->AddNamedInterface( "ClearLog", reinterpret_cast<IInterface::Callback>( 0 ) );

}

///////////////////////////////////////////////////////////////////////////////

EntSkinAssetFilterInterface::EntSkinAssetFilterInterface()
    : CAssetFilterInterface( "skinfilter" )
{
}

///////////////////////////////////////////////////////////////////////////////

struct SkinFilterErr
{
    char buffer[ 1024 ];

    SkinFilterErr( int row, int col, const char *formatstring, ... ) // vararg constructor
    {
        va_list argp;
        va_start( argp, formatstring );
        vsprintf( & buffer[0], formatstring, argp );
        va_end( argp );
        orkprintf( "ERROR: SkinFilterException [err %s] [row %d] [col %d]\n", buffer, row, col );
        //fprintf( stderr, "ERROR: SkinFilterException [err %s]\n", buffer );
        fflush(stdout);
        //fflush(stderr);
    }

    SkinFilterErr( const char *formatstring, ... ) // vararg constructor
    {
        va_list argp;
        va_start( argp, formatstring );
        vsprintf( & buffer[0], formatstring, argp );
        va_end( argp );
        orkprintf( "ERROR: SkinFilterException [err %s]\n", buffer );
        //fprintf( stderr, "ERROR: SkinFilterException [err %s]\n", buffer );
        fflush(stdout);
        //fflush(stderr);
    }

};

struct SkinFilterMissingAttrErr : public SkinFilterErr
{
    SkinFilterMissingAttrErr( int row, int col, const char* element, const char* attr )
        : SkinFilterErr(row, col, "No \"%s\" attribute found on XML element <%s>", attr, element) {}
    SkinFilterMissingAttrErr( const char* element, const char* attr )
        : SkinFilterErr("No \"%s\" attribute found on XML element <%s>", attr, element) {}
};

///////////////////////////////////////////////////////////////////////////////

orkvector<const char*> soundMapSounds;
orkvector<const char*> referencedSounds;

///////////////////////////////////////////////////////////////////////////////

void ParseModelNode( const TiXmlElement *pmdlelem, EntitySkin & skin )
{
    const std::string Asset = pmdlelem->Attribute( "Asset" );

    ork::CModel *pmdl = CModelManager::GetRef().Create( Asset );

    if( 0 == pmdl )
    {
        throw SkinFilterErr( pmdlelem->Row(), pmdlelem->Column(), "ParseModelNode(): <Missing Model Asset %s>", Asset.c_str() );
    }

    for(const TiXmlElement *pXmlElement = pmdlelem->FirstChildElement(); pXmlElement; pXmlElement =
pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "DiffuseColor") == 0 )
        {
            const PropTypeString DifColStr( pXmlElement->Attribute( "Val" ) );
            CVector3 Color = CPropType<CVector3>::FromString( DifColStr );
            pmdl->SetDiffuseColor( Color );
        }
        else if( strcmp(pXmlElement->Value(), "AmbientColor") == 0 )
        {
            const PropTypeString DifColStr( pXmlElement->Attribute( "Val" ) );
            CVector3 Color = CPropType<CVector3>::FromString( DifColStr );
            pmdl->SetAmbientColor( Color );
        }
        else if( strcmp(pXmlElement->Value(), "SpecularColor") == 0 )
        {
            const PropTypeString DifColStr( pXmlElement->Attribute( "Val" ) );
            CVector3 Color = CPropType<CVector3>::FromString( DifColStr );
            pmdl->SetSpecularColor( Color );
        }
        else if( strcmp(pXmlElement->Value(), "EmissiveColor") == 0 )
        {
            const PropTypeString DifColStr( pXmlElement->Attribute( "Val" ) );
            CVector3 Color = CPropType<CVector3>::FromString( DifColStr );
            pmdl->SetEmissiveColor( Color );
        }
        else if( strcmp(pXmlElement->Value(), "Light0") == 0 )
        {
            const PropTypeString Enable( pXmlElement->Attribute( "Val" ) );
            bool bena = CPropType<bool>::FromString( Enable );
            pmdl->LightEnable( 0, bena );
        }
        else if( strcmp(pXmlElement->Value(), "Light1") == 0 )
        {
            const PropTypeString Enable( pXmlElement->Attribute( "Val" ) );
            bool bena = CPropType<bool>::FromString( Enable );
            pmdl->LightEnable( 1, bena );
        }
        else if( strcmp(pXmlElement->Value(), "Light2") == 0 )
        {
            const PropTypeString Enable( pXmlElement->Attribute( "Val" ) );
            bool bena = CPropType<bool>::FromString( Enable );
            pmdl->LightEnable( 2, bena );
        }
        else if( strcmp(pXmlElement->Value(), "Light3") == 0 )
        {
            const PropTypeString Enable( pXmlElement->Attribute( "Val" ) );
            bool bena = CPropType<bool>::FromString( Enable );
            pmdl->LightEnable( 3, bena );
        }
        else if( strcmp(pXmlElement->Value(), "ToonShade") == 0 )
        {
            const PropTypeString Enable( pXmlElement->Attribute( "Val" ) );
            bool bena = CPropType<bool>::FromString( Enable );
            pmdl->ToonShadingEnable( bena );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseModelNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }

        skin.SetModel( Asset.c_str() );

    }

}

///////////////////////////////////////////////////////////////////////////////

void ParseAnimEventSeqNode( const TiXmlElement *pelem, EntitySkin & skin, AnimSeqEventTable* ptab )
{
    OrkAssert(ptab);

    const char *SoundOneShot = pelem->Attribute( "SoundOneShot" );
    const char *SoundLoopOn = pelem->Attribute( "SoundLoopOn" );
    const char *SoundLoopOff = pelem->Attribute( "SoundLoopOff" );
    const char *CollisionOn = pelem->Attribute( "CollisionOn" );
    const char *CollisionOff = pelem->Attribute( "CollisionOff" );
    const char *Explosion = pelem->Attribute( "Explosion" );
    const char *Projectile = pelem->Attribute( "Projectile" );
    const char *Frame = pelem->Attribute( "Frame" );
    const char *ComboWindow = pelem->Attribute("ComboWindow");

    if( 0 == Frame ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Event", "Frame" );

    bool parsed = false;

    struct X
    {
        static bool SetParsed(bool &parsed, const TiXmlElement *pXmlElement)
        {
            if(parsed)
                throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseAnimEventSeqNode(): Event specifies more than
one action");

            parsed = true;

            return true;
        }
    };

    if( SoundOneShot && X::SetParsed(parsed, pelem))
    {
        // sm - add referenced sound
        referencedSounds.push_back(SoundOneShot);

        SoundEvent* evt = OrkNew SoundEvent();
        evt->SetSoundName(CStringTable::RefTopStringTable().AddString(SoundOneShot));
        evt->SetSoundOn(true);
        evt->SetSoundLooping(false);
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( SoundLoopOn && X::SetParsed(parsed, pelem) )
    {
        SoundEvent* evt = OrkNew SoundEvent();
        evt->SetSoundName(CStringTable::RefTopStringTable().AddString(SoundOneShot));
        evt->SetSoundOn(true);
        evt->SetSoundLooping(true);
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( SoundLoopOff && X::SetParsed(parsed, pelem) )
    {
        SoundEvent* evt = OrkNew SoundEvent();
        evt->SetSoundName(CStringTable::RefTopStringTable().AddString(SoundOneShot));
        evt->SetSoundOn(false);
        evt->SetSoundLooping(true);
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( CollisionOn && X::SetParsed(parsed, pelem) )
    {
        CollisionEvent* evt = OrkNew CollisionEvent();
        evt->SetObjectName(CStringTable::RefTopStringTable().AddString(CollisionOn));
        evt->SetActivate(true);
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( CollisionOff && X::SetParsed(parsed, pelem) )
    {
        CollisionEvent* evt = OrkNew CollisionEvent();
        evt->SetObjectName(CStringTable::RefTopStringTable().AddString(CollisionOn));
        evt->SetActivate(false);
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( Explosion && X::SetParsed(parsed, pelem) )
    {
        ExplosionEvent* evt = OrkNew ExplosionEvent();
        evt->SetExplosionName(CStringTable::RefTopStringTable().AddString(Explosion));
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( Projectile && X::SetParsed(parsed, pelem) )
    {
        ProjectileEvent* evt = OrkNew ProjectileEvent();
        evt->SetProjectileName(CStringTable::RefTopStringTable().AddString(Projectile));
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }

    if( ComboWindow && X::SetParsed(parsed, pelem))
    {
        ComboWindowEvent* evt = OrkNew ComboWindowEvent();
        evt->SetComboWindowName(CStringTable::RefTopStringTable().AddString(ComboWindow));
        ptab->AddEvent(CPropType<float>::FromString(Frame), evt);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ParseAnimNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    const char *Key = pelem->Attribute( "Key" );
    const char *Asset = pelem->Attribute( "Asset" );

    if( 0 == Key ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Anim", "Key" );
    if( 0 == Asset ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Anim", "Asset" );

    skin.SetAnim( Key, Asset );

    AnimSeqEventTable* ptab = NULL;

    for(const TiXmlElement *pXmlElement = pelem->FirstChildElement(); pXmlElement; pXmlElement = pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "Event") == 0 )
        {
            if(NULL == ptab)
                ptab = OrkNew AnimSeqEventTable();

            ParseAnimEventSeqNode( pXmlElement, skin, ptab );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseAnimNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }
    }

    if(ptab)
        skin.SetAnimSeqEventTable(Key, ptab);
}

///////////////////////////////////////////////////////////////////////////////

void ParseAnimMapNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    for(const TiXmlElement *pXmlElement = pelem->FirstChildElement(); pXmlElement; pXmlElement = pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "Anim") == 0 )
        {
            ParseAnimNode( pXmlElement, skin );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseAnimMapNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void ParseSoundNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    const char * Key = pelem->Attribute( "Key" );
    const char * Asset = pelem->Attribute( "Asset" );

    if( 0 == Key ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sound", "Key" );
    if( 0 == Asset ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sound", "Asset" );

    soundMapSounds.push_back(Key);

    skin.AddSoundProgram( Key, Asset );
}

///////////////////////////////////////////////////////////////////////////////

void ParseSoundMapNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    for(const TiXmlElement *pXmlElement = pelem->FirstChildElement(); pXmlElement; pXmlElement = pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "Sound") == 0 )
        {
            ParseSoundNode( pXmlElement, skin );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseSoundMapNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }
    }

}

///////////////////////////////////////////////////////////////////////////////

void ParseSphereNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    std::string		GroupName = pelem->Attribute( "GroupName" );

    std::string		Joint;
    bool			GiveDamage;
    bool			TakeDamage;
    bool			StopMotion;
    bool			DefaultActive;
    float			Multiplier;
    float			Radius;
    CVector3		Offset;

    const char *JointVal = pelem->Attribute( "Joint" );
    const char *TakeDamageVal = pelem->Attribute( "TakeDamage" );
    const char *GiveDamageVal = pelem->Attribute( "GiveDamage" );
    const char *StopMotionVal = pelem->Attribute( "StopMotion" );
    const char *DefActiveVal = pelem->Attribute( "DefaultActive" );
    const char *MultiplierVal = pelem->Attribute( "Multiplier" );
    const char *RadiusVal = pelem->Attribute( "Radius" );
    const char *OffsetVal = pelem->Attribute( "Offset" );

    if( 0 == JointVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "Joint" );
    if( 0 == TakeDamageVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "TakeDamage" );
    if( 0 == GiveDamageVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "GiveDamage" );
    if( 0 == StopMotionVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "StopMotion" );
    if( 0 == DefActiveVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "DefaultActive" );
    if( 0 == MultiplierVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "Multiplier" );
    if( 0 == RadiusVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "Radius" );
    if( 0 == OffsetVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), "Sphere", "Offset" );

    Joint =			JointVal;
    TakeDamage =	CPropType<bool>::FromString( TakeDamageVal );
    GiveDamage =	CPropType<bool>::FromString( GiveDamageVal );
    StopMotion =	CPropType<bool>::FromString( StopMotionVal );
    DefaultActive = CPropType<bool>::FromString( DefActiveVal );
    Multiplier =	CPropType<float>::FromString( MultiplierVal );
    Radius =		CPropType<float>::FromString( RadiusVal );
    Offset =		CPropType<CVector3>::FromString( OffsetVal );

    skin.AddCollisionSphere( GroupName.c_str(), Joint.c_str(), Radius, Offset, TakeDamage, GiveDamage, StopMotion, Multiplier,
DefaultActive );

}
template<typename T>
static T ReadAttribute( const TiXmlElement *pelem, const char *node, const char *name)
{
    const char *Val = pelem->Attribute( name );
    if( 0 == Val ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), node, name );
    return CPropType<T>::FromString( Val );
}

void ParseCylinderNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    std::string		GroupName = ReadAttribute<std::string>(pelem, "Cylinder", "GroupName");
    std::string		Joint = ReadAttribute<std::string>(pelem, "Cylinder", "Joint");
    bool			GiveDamage = ReadAttribute<bool>(pelem, "Cylinder", "GiveDamage");
    bool			TakeDamage = ReadAttribute<bool>(pelem, "Cylinder", "TakeDamage");
    bool			StopMotion = ReadAttribute<bool>(pelem, "Cylinder", "StopMotion");
    bool			DefaultActive = ReadAttribute<bool>(pelem, "Cylinder", "DefaultActive");
    float			Multiplier = ReadAttribute<float>(pelem, "Cylinder", "Multiplier");
    float			Radius = ReadAttribute<float>(pelem, "Cylinder", "Radius");
    CVector3		Start = ReadAttribute<CVector3>(pelem, "Cylinder", "Start");
    CVector3		End = ReadAttribute<CVector3>(pelem, "Cylinder", "End");

    CVector3 delta = End - Start;
    float distance = delta.Mag();
    float countspheres = float::Ceil( distance/(float(1.5f)*Radius) );
    int numspheres = countspheres.NumericCast();

    for(int i = 0; i < numspheres; i++)
    {
        skin.AddCollisionSphere( GroupName.c_str(), Joint.c_str(), Radius, Start + delta*(float(i)/countspheres), TakeDamage,
GiveDamage, StopMotion, Multiplier, DefaultActive );
    }
}

///////////////////////////////////////////////////////////////////////////////

void ParseCollisionMapNode( const TiXmlElement *pelem, EntitySkin & skin )
{
    for(const TiXmlElement *pXmlElement = pelem->FirstChildElement(); pXmlElement; pXmlElement = pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "Sphere") == 0 )
        {
            ParseSphereNode( pXmlElement, skin );
        }
        else if( strcmp(pXmlElement->Value(), "Cylinder") == 0 )
        {
            ParseCylinderNode( pXmlElement, skin );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseCollisionMapNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }
    }

}

///////////////////////////////////////////////////////////////////////////////

orkvector<const char *> GroupStack;

void ParseUserMapNode( const TiXmlElement *pelem, EntitySkin & skin, bool bgroup )
{
    if( bgroup )
    {
        const char *GroupName = pelem->Attribute( "Name" );
        GroupStack.push_back( GroupName );
    }

    for(const TiXmlElement *pXmlElement = pelem->FirstChildElement(); pXmlElement; pXmlElement = pXmlElement->NextSiblingElement())
    {
        if( strcmp(pXmlElement->Value(), "Data") == 0 )
        {
            ///////////////////////////////////////////////////
            const char *KeyVal = pXmlElement->Attribute( "Key" );
            const char *ValVal = pXmlElement->Attribute( "Value" );

            if( 0 == KeyVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), pelem->Value(), "Key" );
            if( 0 == ValVal ) throw SkinFilterMissingAttrErr( pelem->Row(), pelem->Column(), pelem->Value(), "Value" );


            if(strcmp(KeyVal, "Sound") == 0)
                referencedSounds.push_back(ValVal);

            std::string KeyGroup = "";

            for( orkvector<const char *>::iterator it=GroupStack.begin(); it!=GroupStack.end(); it++ )
            {
                KeyGroup += std::string(*it) + ".";
            }

            skin.AddUserDataItem( (KeyGroup+KeyVal).c_str(), ValVal );

            ///////////////////////////////////////////////////
        }
        else if( strcmp(pXmlElement->Value(), "Group") == 0 )
        {
            ParseUserMapNode( pXmlElement, skin, true );
        }
        else
        {
            throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "ParseUserMapNode(): <Invalid Node Name %s>",
pXmlElement->Value() );
        }
    }

    if( bgroup )
    {
        GroupStack.pop_back();
    }

}

///////////////////////////////////////////////////////////////////////////////

bool EntSkinAssetFilterInterface::ConvertAsset( const std::string & FromFileName, const std::string & ToFileName )
{
    CMemoryManager::GetCurrentMemoryManager().Push();

    EntitySkin Skin;

    try
    {
        TiXmlDocument XmlDoc;

        bool bok = XmlDoc.LoadFile( FromFileName );

        if( false == bok )
        {
            const char *ErrorDesc = XmlDoc.ErrorDesc();
            int irow = XmlDoc.ErrorRow();
            int icol = XmlDoc.ErrorCol();
            //XmlDoc.Err
            throw SkinFilterErr( "< Xml File Not Loaded [row %d] [col %d] [desc %s] >", irow, icol, ErrorDesc );
        }

        TiXmlNode* RootNode = XmlDoc.FirstChild( "Skin" );

        if( 0 == RootNode ) throw SkinFilterErr( "<No Top Level Skin Node>" );

        const TiXmlElement *SkinNode = RootNode->ToElement();

        if( 0 == SkinNode )
        {
            throw SkinFilterErr( "<No Top Level Skin Node>" );
        }
        else
        {
            const char * AssetName = SkinNode->Attribute( "AssetName" );

            std::string AssetNameStr = CFileEnv::filespec_to_native( AssetName );

            if( 0 == AssetName )
            {
                throw SkinFilterErr( "<Skin Node has no AssetName>" );
            }

            int findit = FromFileName.find( AssetNameStr );

            if( findit == FromFileName.npos )
            {
                throw SkinFilterErr( "<Check AssetName attr on the SkinNode> < %s not found in %s >", AssetNameStr.c_str(),
FromFileName.c_str() );
            }
            else
            {
                std::string nextchk = FromFileName.substr( findit );
                nextchk = CFileEnv::filespec_no_extension( nextchk );

                if( nextchk != AssetNameStr )
                {
                    throw SkinFilterErr( "<Check AssetName attr on the SkinNode> < %s != %s >", nextchk.c_str(),
AssetNameStr.c_str() );
                }
            }

            soundMapSounds.clear();
            referencedSounds.clear();

            for(const TiXmlElement *pXmlElement = SkinNode->FirstChildElement(); pXmlElement; pXmlElement =
pXmlElement->NextSiblingElement())
            {
                if( strcmp(pXmlElement->Value(), "Model") == 0 )
                {
                    ParseModelNode( pXmlElement, Skin );
                }
                else if( strcmp(pXmlElement->Value(), "AnimMap") == 0 )
                {
                    ParseAnimMapNode( pXmlElement, Skin );
                }
                else if( strcmp(pXmlElement->Value(), "SoundMap") == 0 )
                {
                    ParseSoundMapNode( pXmlElement, Skin );
                }
                else if( strcmp(pXmlElement->Value(), "CollisionMap") == 0 )
                {
                    ParseCollisionMapNode( pXmlElement, Skin );
                }
                else if( strcmp(pXmlElement->Value(), "User") == 0 )
                {
                    ParseUserMapNode( pXmlElement, Skin, false );
                }
                else
                {
                    throw SkinFilterErr( pXmlElement->Row(), pXmlElement->Column(), "<Invalid Node Name %s> <LINE %s>",
pXmlElement->Value(), __LINE__ );
                }
            }

            orkprintf("-*-*- %s Sound Map: -*-*-\n", AssetName);
            orkvector<const char*>::iterator it = soundMapSounds.begin();
            while(it != soundMapSounds.end())
            {
                bool referenced = false;

                orkprintf("Sound: %s ", *it);

                orkvector<const char*>::iterator rit = referencedSounds.begin();
                while(rit != referencedSounds.end())
                {
                    if(strcmp(*rit, *it) == 0)
                        referenced = true;

                    rit++;
                }

                if(!referenced)
                    orkprintf("NOT REFERENCED\n");
                else
                    orkprintf("Referenced\n");

                it++;
            }
            orkprintf("-*-*- End Sound Map -*-*-\n\n");

            Skin.SetAssetName( AssetName );

            AnimSkinLoader::Save( & Skin, ToFileName.c_str() );
        }
    }
    catch( SkinFilterErr & )
    {
        throw std::exception();
    }
    //sushiApplication->PopFileEnvContext();
    CMemoryManager::GetCurrentMemoryManager().Pop();

    return true;

}
///////////////////////////////////////////////////////////////////////////////
*/

}} // namespace ork::tool
