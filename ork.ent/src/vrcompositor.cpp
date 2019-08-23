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
#include <ork/lev2/gfx/gfxprimitives.h>


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

#if defined(ENABLE_VR)

fmtx4 steam34tofmtx4( const vr::HmdMatrix34_t &matPose )
{
    fmtx4 orkmtx = fmtx4::Identity;
    for( int i=0; i<3; i++ )
      for( int j=0; j<4; j++ )
        orkmtx.SetElemXY(j,i,matPose.m[i][j]);
	  return orkmtx;
}
fmtx4 steam44tofmtx4( const vr::HmdMatrix44_t &matPose )
{
    fmtx4 orkmtx;
    for( int i=0; i<4; i++ )
      for( int j=0; j<4; j++ )
        orkmtx.SetElemXY(j,i,matPose.m[i][j]);
	  return orkmtx;
}

std::string trackedDeviceString( vr::TrackedDeviceIndex_t unDevice,
                                 vr::TrackedDeviceProperty prop,
                                 vr::TrackedPropertyError *peError = NULL )
{
	uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty( unDevice, prop, NULL, 0, peError );
	if( unRequiredBufferLen == 0 )
		return "";

	char *pchBuffer = new char[ unRequiredBufferLen ];
	unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty( unDevice, prop, pchBuffer, unRequiredBufferLen, peError );
	std::string sResult = pchBuffer;
	delete [] pchBuffer;
	return sResult;
}
#endif
///////////////////////////////////////////////////////////////////////////

struct ControllerState {
  fmtx4 _matrix;
  bool _button1down = false;
  bool _button2down = false;
};

///////////////////////////////////////////////////////////////////////////

constexpr int NUMSAMPLES = 16;

struct VrFrameTechnique final : public FrameTechniqueBase
{
    VrFrameTechnique(int w, int h)
      : FrameTechniqueBase(w,h)
      , _rtg_left(nullptr)
      , _rtg_right(nullptr)
    {
    }


    void DoInit( GfxTarget* pTARG ) final {
        if(nullptr==_rtg_left){
            _rtg_left = new RtGroup( pTARG, miW, miH, NUMSAMPLES );
            _rtg_right = new RtGroup( pTARG, miW, miH, NUMSAMPLES );

            auto lbuf = new RtBuffer( _rtg_left,
                                      lev2::ETGTTYPE_MRT0,
                                      lev2::EBUFFMT_RGBA32,
                                      miW, miH );
            auto rbuf = new RtBuffer( _rtg_right,
                                      lev2::ETGTTYPE_MRT0,
                                      lev2::EBUFFMT_RGBA32,
                                      miW, miH );

            _rtg_left->SetMrt( 0, lbuf );
            _rtg_right->SetMrt( 0, rbuf );

            _effect.PostInit( pTARG, "orkshader://framefx", "frameeffect_standard" );

        }
    }
    void renderBothEyes( FrameRenderer& renderer,
                         CMCIdrawdata& drawdata,
                         CCameraData* lcam,
                         CCameraData* rcam,
                         const std::map<int,ControllerState>& controllers ) {
      RenderContextFrameData&	FrameData = renderer.GetFrameData();
    	GfxTarget *pTARG = FrameData.GetTarget();

      SRect tgt_rect( 0, 0, miW, miH );

      _CPD.mbDrawSource = true;
      _CPD.mpFrameTek = this;
      _CPD.mpCameraName = nullptr;
      _CPD.mpLayerName = nullptr; // default == "All"

      //////////////////////////////////////////////////////
      // render all controller poses
      //////////////////////////////////////////////////////

      auto renderposes = [&](CCameraData* camdat){

        fmtx4 rx;
        fmtx4 ry;
        fmtx4 rz;
        rx.SetRotateX(-PI*0.5);
        ry.SetRotateY(PI*0.5);
        rz.SetRotateZ(PI*0.5);

        for( auto item : controllers ){

          auto c = item.second;

          fmtx4 scalemtx;
          scalemtx.SetScale(c._button1down ? 0.1 : 0.05 );

          pTARG->MTXI()->PushMMatrix( scalemtx*rx*ry*rz*c._matrix );
          pTARG->MTXI()->PushVMatrix( camdat->GetVMatrix() );
          pTARG->MTXI()->PushPMatrix( camdat->GetPMatrix() );
          pTARG->PushModColor( CVector4::White() );
          {
            if( c._button2down )
              ork::lev2::CGfxPrimitives::GetRef().RenderBox( pTARG );
            else
              ork::lev2::CGfxPrimitives::GetRef().RenderAxis( pTARG );
          }
          pTARG->PopModColor();
          pTARG->MTXI()->PopPMatrix();
          pTARG->MTXI()->PopVMatrix();
          pTARG->MTXI()->PopMMatrix();

        }
      };

      //////////////////////////////////////////////////////

      pTARG->FBI()->SetAutoClear(false);
      // clear will occur via _CPD
      // draw left ////////////////////////////////////////

      lcam->BindGfxTarget(pTARG);
      FrameData.SetCameraData(lcam);
      _CPD._impl.Set<const CCameraData*>(lcam);
      _CPD._clearColor = fvec4(0,0,.1,1);

      RtGroupRenderTarget rtL(_rtg_left);
      drawdata.mCompositingGroupStack.push(_CPD);{
          pTARG->SetRenderContextFrameData( & FrameData );
          FrameData.SetDstRect( tgt_rect );
          FrameData.PushRenderTarget(&rtL);
          pTARG->FBI()->PushRtGroup( _rtg_left );
          pTARG->BeginFrame();
              FrameData.SetRenderingMode( RenderContextFrameData::ERENDMODE_STANDARD );
              renderer.Render();
              renderposes(lcam);
          pTARG->EndFrame();
          pTARG->FBI()->PopRtGroup();
          FrameData.PopRenderTarget();
          pTARG->SetRenderContextFrameData( nullptr );
          drawdata.mCompositingGroupStack.pop();
      }

      // draw right ///////////////////////////////////////

      rcam->BindGfxTarget(pTARG);
      FrameData.SetCameraData(rcam);
      _CPD._impl.Set<const CCameraData*>(rcam);
      _CPD._clearColor = fvec4(0,0,.1,1);

      drawdata.mCompositingGroupStack.push(_CPD);{
          RtGroupRenderTarget rtR(_rtg_right);
          pTARG->SetRenderContextFrameData( & FrameData );
          FrameData.SetDstRect( tgt_rect );
          FrameData.PushRenderTarget(&rtR);
          pTARG->FBI()->PushRtGroup( _rtg_right );
          pTARG->BeginFrame();
              FrameData.SetRenderingMode( RenderContextFrameData::ERENDMODE_STANDARD );
              renderer.Render();
              renderposes(rcam);
          pTARG->EndFrame();
          pTARG->FBI()->PopRtGroup();
          FrameData.PopRenderTarget();
          pTARG->SetRenderContextFrameData( nullptr );
        }

    }

    RtGroup* _rtg_left;
    RtGroup*	_rtg_right;
    BuiltinFrameEffectMaterial _effect;
    ent::CompositingPassData _CPD;

};

///////////////////////////////////////////////////////////////////////////////
struct VRSYSTEMIMPL {
  ///////////////////////////////////////
  VRSYSTEMIMPL()
    : _frametek(nullptr)
    , _camname(AddPooledString("Camera"))
    , _layers(AddPooledString("All"))
    , _active(false)
    , _width(1024)
    , _height(1024) {

    #if defined(ENABLE_VR)
      vr::EVRInitError error = vr::VRInitError_None;
      _hmd = vr::VR_Init( &error, vr::VRApplication_Scene );
      _active = (error==vr::VRInitError_None);

      if( _active ){
          _hmd->GetRecommendedRenderTargetSize( &_width, &_height );
          printf( "RECOMMENDED WH<%d %d>\n", _width, _height );
          auto str_driver = trackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
        	auto str_display = trackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );
          printf( "str_driver<%s>\n", str_driver.c_str() );
          printf( "str_driver<%s>\n", str_display.c_str() );
          auto proj_mtx_l = steam44tofmtx4(_hmd->GetProjectionMatrix( vr::Eye_Left, .1, 10000.0f ));
          auto proj_mtx_r = steam44tofmtx4(_hmd->GetProjectionMatrix( vr::Eye_Right, .1, 10000.0f ));
          auto eyep_mtx_l = steam34tofmtx4(_hmd->GetEyeToHeadTransform( vr::Eye_Left ));
          auto eyep_mtx_r = steam34tofmtx4(_hmd->GetEyeToHeadTransform( vr::Eye_Right ));

          _posemap["projl"] = proj_mtx_l;
          _posemap["projr"] = proj_mtx_r;
          _posemap["eyel"].GEMSInverse(eyep_mtx_l);
          _posemap["eyer"].GEMSInverse(eyep_mtx_r);
    }
    #endif
    _leftcamera.SetWidth(_width);
    _leftcamera.SetHeight(_height);
    _rightcamera.SetWidth(_width);
    _rightcamera.SetHeight(_height);

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

    _frametek = new VrFrameTechnique(_width,_height);
    _frametek->Init( pTARG );


  }
  ///////////////////////////////////////
  void _myrender( FrameRenderer& renderer, CMCIdrawdata& drawdata, fmtx4 rootmatrix ) {

      fmtx4 hmd = _posemap["hmd"];
      fmtx4 eyeL = _posemap["eyel"];
      fmtx4 eyeR = _posemap["eyer"];

      fmtx4 lmv = rootmatrix*hmd*eyeL;
      fmtx4 rmv = rootmatrix*hmd*eyeR;

      _leftcamera.SetView(lmv);
      _leftcamera.setCustomProjection(_posemap["projl"]);
      _rightcamera.SetView(rmv);
      _rightcamera.setCustomProjection(_posemap["projr"]);
      _frametek->renderBothEyes(renderer,
                                drawdata,
                                &_leftcamera,
                                &_rightcamera,
                                _controllers );
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
	VrFrameTechnique*	_frametek;
  uint32_t _width, _height;
  std::map<std::string,fmtx4> _posemap;
  CCameraData _leftcamera;
  CCameraData _rightcamera;
  std::map<int,ControllerState> _controllers;
  bool _active;
  # if defined(ENABLE_VR)
    vr::IVRSystem* _hmd;
    vr::TrackedDevicePose_t _trackedPoses[ vr::k_unMaxTrackedDeviceCount ];
    fmtx4 _poseMatrices[ vr::k_unMaxTrackedDeviceCount ];
    std::string _devclass[ vr::k_unMaxTrackedDeviceCount ];
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

    # if defined(ENABLE_VR)
    if( vrimpl->_active ){
      bool ovr_compositor_ok = (bool) vr::VRCompositor();
      assert(ovr_compositor_ok);
    }
    #endif


	if( nullptr == vrimpl->_frametek )
	{
    vrimpl->init(pTARG);
	}


}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) // virtual
{
    auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
    static PoolString vrcamname = AddPooledString("vrcam");

    //////////////////////////////////////////////
    // find vr camera
    //////////////////////////////////////////////

    auto psi = pCCI->sceneInst();
    auto vrcam = psi->GetCameraData(vrcamname);

    fmtx4 rootmatrix;

    if( vrcam != nullptr ){
      auto eye = vrcam->GetEye();
      auto tgt = vrcam->GetTarget();
      auto up = vrcam->GetUp();
      rootmatrix.LookAt(eye,tgt,up);
    }


    //////////////////////////////////////////////
    // process OpenVR events
    //////////////////////////////////////////////
    # if defined(ENABLE_VR)

    vr::VREvent_t event;
  	while( vrimpl->_active and vrimpl->_hmd->PollNextEvent( &event, sizeof( event ) ) )
  	{
       auto data = event.data;
       auto ctrl = data.controller;
       int button = ctrl.button;

	     switch( event.eventType ) {
	        case vr::VREvent_TrackedDeviceDeactivated:
			       printf( "Device %u detached.\n", event.trackedDeviceIndex );
		         break;
	        case vr::VREvent_TrackedDeviceUpdated:
		         break;
         case vr::VREvent_ButtonPress:{
             printf( "button<%d> pressed\n", button);
             auto& c = vrimpl->_controllers[event.trackedDeviceIndex];
             switch(button){
               case 1:
                c._button1down = true;
                break;
               case 2:
                c._button2down = true;
                break;
             }
   		       break;
         }
         case vr::VREvent_ButtonUnpress:{
             printf( "button<%d> released\n", button);
             auto& c = vrimpl->_controllers[event.trackedDeviceIndex];
             switch(button){
               case 1:
                c._button1down = false;
                break;
               case 2:
                c._button2down = false;
                break;
             }
   		       break;
         }
         case vr::VREvent_ButtonTouch:
             printf( "button<%d> touched\n", button);
   		       break;
         case vr::VREvent_ButtonUntouch:
             printf( "button<%d> untouched\n", button);
   		       break;
          default:
             break;
	     }
  	}

    //////////////////////////////////////////////
    if( vrimpl->_active ){
        vr::VRActionSetHandle_t actset_demo = vr::k_ulInvalidActionSetHandle;
        vr::VRActiveActionSet_t actionSet = { 0 };
    	  actionSet.ulActionSet = actset_demo;
    	  vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );
    }
    //////////////////////////////////////////////

    #endif


  	//const ent::CompositingGroup* pCG = _group;
  	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();


  	if(vrimpl->_frametek ) {

        /////////////////////////////////////////////////////////////////////////////
        // render eyes
        /////////////////////////////////////////////////////////////////////////////

    		anyp PassData;
    		PassData.Set<const char*>( "All" );
    		the_renderer.GetFrameData().SetUserProperty( "pass", PassData );
    		vrimpl->_myrender( the_renderer, drawdata, rootmatrix );

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

            # if defined(ENABLE_VR)

            if( vrimpl->_active ){

                vr::Texture_t leftEyeTexture = {
                    (void*)(uintptr_t)texobjL,
                    vr::TextureType_OpenGL,
                    vr::ColorSpace_Gamma
                };
                vr::Texture_t rightEyeTexture = {
                    (void*)(uintptr_t)texobjR,
                    vr::TextureType_OpenGL,
                    vr::ColorSpace_Gamma
                };

                //////////////////////////////////////////////////
                // odd that you need to set the viewport
                //  before submitting to the vr compositor,
                //  since the texture contains the size of itself...
                //////////////////////////////////////////////////

                SRect VPRect( 0, 0, vrimpl->_width, vrimpl->_height );
                auto targ = framedata.GetTarget();
              	targ->FBI()->PushViewport( VPRect );
              	targ->FBI()->PushScissor( VPRect );

                //////////////////////////////////////////////////
                // submit to openvr compositor
                //////////////////////////////////////////////////

                GLuint erl = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
                //assert(erl==GL_NO_ERROR);
                GLuint err = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );
                //assert(err==GL_NO_ERROR);

                //////////////////////////////////////////////////
                // undo above PushVp/Scissor
                //////////////////////////////////////////////////

                targ->FBI()->PopViewport( );
              	targ->FBI()->PopScissor( );

            }

            //////////////////////////////////////////////////

            #endif


        }
        /////////////////////////////////////////////////////////////////////////////

  	}
    # if defined(ENABLE_VR)

    auto hmd = vrimpl->_hmd;

    if( vrimpl->_active and hmd->IsInputAvailable() ){

      vr::VRCompositor()->WaitGetPoses( vrimpl->_trackedPoses, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

    	int validposecount = 0;
    	std::string pose_classes = "";

    	for ( int dev_index = 0;
                dev_index<vr::k_unMaxTrackedDeviceCount;
                dev_index++ ) {
    		if ( vrimpl->_trackedPoses[dev_index].bPoseIsValid ){
    			validposecount++;
    			auto orkmtx = steam34tofmtx4( vrimpl->_trackedPoses[dev_index].mDeviceToAbsoluteTracking );
          fmtx4 inverse;
          fmtx4 transpose = orkmtx;
          transpose.Transpose();
          inverse.GEMSInverse(orkmtx);
          vrimpl->_poseMatrices[dev_index] = orkmtx;
    			//if (vrimpl->_devclass[dev_index]==0){
    				switch (hmd->GetTrackedDeviceClass(dev_index)){
        				case vr::TrackedDeviceClass_Controller:
                    vrimpl->_devclass[dev_index] = 'C';
                    vrimpl->_controllers[dev_index]._matrix = orkmtx;
                    break;
        				case vr::TrackedDeviceClass_HMD:
                    vrimpl->_devclass[dev_index] = 'H';
                    break;
        				case vr::TrackedDeviceClass_Invalid:
                    vrimpl->_devclass[dev_index] = 'I';
                    break;
        				case vr::TrackedDeviceClass_GenericTracker:
                    vrimpl->_devclass[dev_index] = 'G';
                    break;
        				case vr::TrackedDeviceClass_TrackingReference:
                    vrimpl->_devclass[dev_index] = 'T';
                    break;
        				default:
                    vrimpl->_devclass[dev_index] = '?';
                    break;
    				}
    			//}
    			pose_classes += vrimpl->_devclass[dev_index];
    		}
    	}
      if ( vrimpl->_trackedPoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid ){
    		vrimpl->_posemap["hmd"].GEMSInverse(vrimpl->_poseMatrices[vr::k_unTrackedDeviceIndex_Hmd]);
    	}

      //printf( "pose_classes<%s>\n", pose_classes.c_str() );
    }

    #endif // ENABLE_VR
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
