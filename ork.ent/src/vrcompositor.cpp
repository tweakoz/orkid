////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

# if ! defined(__APPLE__)

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/Compositor.h>
#include <openvr/openvr.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::VrCompositingNode, "VrCompositingNode");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::Describe()
{
	ork::reflect::RegisterProperty( "Group",
									& VrCompositingNode::GetGroup,
									& VrCompositingNode::SetGroup );
	ork::reflect::AnnotatePropertyForEditor<VrCompositingNode>("Group", "editor.factorylistbase", "CompositingGroup" );

}
///////////////////////////////////////////////////////////////////////////////
struct VRSYSTEMIMPL {
  VRSYSTEMIMPL()
    : _hmd(nullptr){

    vr::EVRInitError error = vr::VRInitError_None;
    _hmd = vr::VR_Init( &error, vr::VRApplication_Scene );
    assert(error==vr::VRInitError_None);


  }
  ~VRSYSTEMIMPL(){
    if( _hmd ){
      vr::VR_Shutdown();
    }
  }
  vr::IVRSystem* _hmd;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode()
	: mFTEK(nullptr)
	, mGroup(nullptr)
{
  _impl = std::make_shared<VRSYSTEMIMPL>();
}
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode()
{
	if( mFTEK ) delete mFTEK;
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::GetGroup(ork::rtti::ICastable*& val) const
{
	CompositingGroup* nonconst = const_cast< CompositingGroup* >( mGroup );
	val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::SetGroup( ork::rtti::ICastable* const & val)
{
	ork::rtti::ICastable* ptr = val;
	mGroup = ( (ptr==0) ? 0 : rtti::safe_downcast<CompositingGroup*>(ptr) );
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoInit( lev2::GfxTarget* pTARG, int iW, int iH ) // virtual
{
	if( nullptr == mFTEK )
	{
		mCompositingMaterial.Init( pTARG );

		mFTEK = new lev2::BuiltinFrameTechniques( iW,iH );
		mFTEK->Init( pTARG );
	}
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) // virtual
{
    auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();

    //////////////////////////////////////////////
    // process OpenVR events
    //////////////////////////////////////////////

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

    //////////////////////////////////////////////
    //vr::VRActiveActionSet_t actionSet = { 0 };
	  //actionSet.ulActionSet = m_actionsetDemo;
	  //vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );
    //////////////////////////////////////////////

  	const ent::CompositingGroup* pCG = mGroup;
  	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
  	orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;

  	ent::CompositingPassData node;
  	node.mbDrawSource = (pCG != 0);

  	if( mFTEK ) {
    		mFTEK->mfSourceAmplitude = pCG ? 1.0f : 0.0f;
    		anyp PassData;
    		PassData.Set<const char*>( "All" );
    		the_renderer.GetFrameData().SetUserProperty( "pass", PassData );
    		node.mpGroup = pCG;
    		node.mpFrameTek = mFTEK;
    		node.mpCameraName = (pCG!=0) ? & pCG->GetCameraName() : 0;
    		node.mpLayerName = (pCG!=0) ? & pCG->GetLayers() : 0;
    		cgSTACK.push(node);
    		mFTEK->Render( the_renderer );
    		cgSTACK.pop();
  	}
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtGroup* VrCompositingNode::GetOutput() const
{
	lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
	return pRT;
}
///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace ent {

#endif // not apple
