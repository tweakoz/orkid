////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/Compositor.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?

# if ! defined(__APPLE__)
#include <openvr/openvr.h>
#define ENABLE_VR
#endif

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::VrCompositingNode, "VrCompositingNode");

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::Describe(){}
///////////////////////////////////////////////////////////////////////////

constexpr int WIDTH = 1024;
constexpr int HEIGHT = 1024;
constexpr int NUMSAMPLES = 4;

struct VrFrameTechnique final : public FrameTechniqueBase
{
    VrFrameTechnique()
      : FrameTechniqueBase(WIDTH,HEIGHT)
      , _rtg_left(nullptr)
      , _rtg_right(nullptr)
    {

    }


    void DoInit( GfxTarget* pTARG ) final {
        if(nullptr==_rtg_left){
            _rtg_left = new RtGroup( pTARG, WIDTH, HEIGHT, NUMSAMPLES );
            _rtg_right = new RtGroup( pTARG, WIDTH, HEIGHT, NUMSAMPLES );

            auto lbuf = new RtBuffer( _rtg_left,
                                      lev2::ETGTTYPE_MRT0,
                                      lev2::EBUFFMT_RGBA32,
                                      WIDTH, HEIGHT );
            auto rbuf = new RtBuffer( _rtg_right,
                                      lev2::ETGTTYPE_MRT0,
                                      lev2::EBUFFMT_RGBA32,
                                      WIDTH, HEIGHT );

            _rtg_left->SetMrt( 0, lbuf );
            _rtg_right->SetMrt( 0, rbuf );

            _effect.PostInit( pTARG, "orkshader://framefx", "frameeffect_standard" );

        }
    }
    void Render( FrameRenderer& renderer ) final {
      RenderContextFrameData&	FrameData = renderer.GetFrameData();
    	GfxTarget *pTARG = FrameData.GetTarget();
    	SRect tgt_rect( 0, 0, pTARG->GetW(), pTARG->GetH() );
    	FrameData.SetDstRect( tgt_rect );
  		renderer.Render();
    }

    RtGroup* _rtg_left;
    RtGroup*	_rtg_right;
    BuiltinFrameEffectMaterial _effect;

};

///////////////////////////////////////////////////////////////////////////////
struct VRSYSTEMIMPL {
  ///////////////////////////////////////
  VRSYSTEMIMPL()
    : _frametek(nullptr)
    , _camname(AddPooledString("Camera"))
    , _layers(AddPooledString("All")) {

    #if defined(ENABLE_VR)
      vr::EVRInitError error = vr::VRInitError_None;
      _hmd = vr::VR_Init( &error, vr::VRApplication_Scene );
      assert(error==vr::VRInitError_None);
    #endif

  }
  ///////////////////////////////////////
  ~VRSYSTEMIMPL(){

    # if defined(ENABLE_VR)
      if( _hmd )
        vr::VR_Shutdown();
    #endif

    if( _frametek ) delete _frametek;
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG){
    _material.Init( pTARG );

    _frametek = new VrFrameTechnique();
    _frametek->Init( pTARG );
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
	VrFrameTechnique*	_frametek;
  # if defined(ENABLE_VR)
    vr::IVRSystem* _hmd;
  #endif
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode()
{
  _impl = std::make_shared<VRSYSTEMIMPL>();
}
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode(){
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoInit( lev2::GfxTarget* pTARG, int iW, int iH ) // virtual
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();

	if( nullptr == vrimpl->_frametek )
	{
    vrimpl->init(pTARG);
	}
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) // virtual
{
    auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();

    //////////////////////////////////////////////
    // process OpenVR events
    //////////////////////////////////////////////
    # if defined(ENABLE_VR)

    vr::VREvent_t event;
  	while( vrimpl->_hmd->PollNextEvent( &event, sizeof( event ) ) )
  	{
	     switch( event.eventType ) {
	        case vr::VREvent_TrackedDeviceDeactivated:
			       printf( "Device %u detached.\n", event.trackedDeviceIndex );
		         break;
	        case vr::VREvent_TrackedDeviceUpdated:
		         break;
          default:
             break;
	     }
  	}
    #endif

    //////////////////////////////////////////////
    //vr::VRActiveActionSet_t actionSet = { 0 };
	  //actionSet.ulActionSet = m_actionsetDemo;
	  //vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );
    //////////////////////////////////////////////

  	//const ent::CompositingGroup* pCG = _group;
  	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
  	orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;

  	ent::CompositingPassData node;
  	node.mbDrawSource = true;

  	if(vrimpl->_frametek ) {

        /////////////////////////////////////////////////////////////////////////////
        // main view
        /////////////////////////////////////////////////////////////////////////////

    		anyp PassData;
    		PassData.Set<const char*>( "All" );
    		the_renderer.GetFrameData().SetUserProperty( "pass", PassData );
    		//node.mpGroup = pCG;
    		node.mpFrameTek = vrimpl->_frametek;
    		node.mpCameraName = & vrimpl->_camname;
    		node.mpLayerName = & vrimpl->_layers;
    		cgSTACK.push(node);
    		vrimpl->_frametek->Render( the_renderer );
    		cgSTACK.pop();

        /////////////////////////////////////////////////////////////////////////////
        // VR compositor
        /////////////////////////////////////////////////////////////////////////////

        auto bufferL = vrimpl->_frametek->_rtg_left->GetMrt(0);
        assert(bufferL!=nullptr);
        auto bufferR = vrimpl->_frametek->_rtg_right->GetMrt(0);
        assert(bufferR!=nullptr);

        auto ptexL = bufferL->GetTexture();
        auto ptexR = bufferR->GetTexture();
        if( ptexL && ptexR ){

            auto texobjL = ptexL->getProperty<GLuint>("gltexobj");
            auto texobjR = ptexR->getProperty<GLuint>("gltexobj");

            printf( "vrcomposite texl<%p:%u>\n", ptexL, texobjL );
            printf( "vrcomposite texl<%p:%u>\n", ptexR, texobjR );
            # if defined(ENABLE_VR)

            vr::Texture_t leftEyeTexture = {
                (void*)(uintptr_t)texobjL,
                vr::TextureType_OpenGL,
                vr::ColorSpace_Gamma
            };
            vr::Texture_t rightEyeTexture = {
                (void*)(uintptr_t)bufferR,
                vr::TextureType_OpenGL,
                vr::ColorSpace_Gamma
            };

            GLuint erl = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
            //assert(erl==GL_NO_ERROR);
            GLuint err = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );
            //assert(err==GL_NO_ERROR);
            #endif

        }
        /////////////////////////////////////////////////////////////////////////////

  	}
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtGroup* VrCompositingNode::GetOutput() const
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
  if(vrimpl->_frametek )
    return vrimpl->_frametek->_rtg_left;
  else return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace ent {
