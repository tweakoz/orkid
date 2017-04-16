#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
class GridControllerData : public ent::ComponentData
{
    RttiDeclareConcrete( GridControllerData, ent::ComponentData );

    float mfSpinRate;

    void GetTextureAccessor(ork::rtti::ICastable *&model) const;
    void SetTextureAccessor(ork::rtti::ICastable *const &model);
    lev2::TextureAsset*                     mTextureAsset;
    float                                   mfScale;

public:

    lev2::Texture* GetTexture() const;
    float GetScale() const { return mfScale; }
    ent::ComponentInst* CreateComponent(ent::Entity* pent) const final;

    GridControllerData();
    float GetSpinRate() const { return mfSpinRate; }

};

///////////////////////////////////////////////////////////////////////////////

class GridControllerInst : public ent::ComponentInst
{
    RttiDeclareAbstract( GridControllerInst, ent::ComponentInst );

    const GridControllerData&     mCD;
    float                         mPhase;

    void DoUpdate(ent::SceneInst* sinst) final;

public:
    const GridControllerData& GetCD() const { return mCD; }

    GridControllerInst( const GridControllerData& cd, ork::ent::Entity* pent );
    float GetPhase() const { return mPhase; }
};

///////////////////////////////////////////////////////////////////////////////

class GridArchetype : public Archetype
{
    RttiDeclareConcrete( GridArchetype, Archetype );

    void DoLinkEntity( SceneInst* psi, Entity *pent ) const final;
    void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final {}
    void DoCompose(ork::ent::ArchComposer& composer) final;

public:

    GridArchetype();

};
///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
